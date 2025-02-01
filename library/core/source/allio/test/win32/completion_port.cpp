#include <allio/impl/win32/completion_port.hpp>
#include <allio/impl/win32/event.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/impl/win32/wait_packet.hpp>
#include <allio/nothrow/pipe.hpp>
#include <allio/test/spawn.hpp>
#include <allio/test/win32/completion_port.hpp>
#include <allio/win32/kernel_error.hpp>

#include <vsm/defer.hpp>

#include <catch2/catch_all.hpp>

using namespace allio;
using namespace allio::win32;

namespace {

class thread_pool_wait
{
	struct thread_pool
	{
		PTP_POOL m_thread_pool;

		thread_pool()
		{
			m_thread_pool = CreateThreadpool(nullptr);
			REQUIRE(m_thread_pool != nullptr);
		}

		~thread_pool()
		{
			CloseThreadpool(m_thread_pool);
		}

		operator PTP_POOL() const
		{
			return m_thread_pool;
		}
	};

	struct thread_pool_environ : TP_CALLBACK_ENVIRON
	{
		thread_pool_environ()
		{
			InitializeThreadpoolEnvironment(this);
		}

		~thread_pool_environ()
		{
			DestroyThreadpoolEnvironment(this);
		}
	};

	thread_pool m_thread_pool;
	thread_pool_environ m_environ;

	PTP_WAIT m_wait = nullptr;
	vsm::atomic<TP_WAIT_RESULT> m_result = WAIT_OBJECT_0 + 1;

public:
	thread_pool_wait()
	{
		SetThreadpoolCallbackPool(&m_environ, m_thread_pool);

		m_wait = CreateThreadpoolWait(
			static_callback,
			this,
			&m_environ);

		REQUIRE(m_wait != nullptr);
	}

	thread_pool_wait(thread_pool_wait const&) = delete;
	thread_pool_wait& operator=(thread_pool_wait const&) = delete;

	~thread_pool_wait()
	{
		CloseThreadpoolWait(m_wait);
	}

	void set(
		HANDLE const handle,
		std::chrono::duration<int64_t, std::ratio<1, 10'000'000>> const timeout)
	{
		LARGE_INTEGER timeout_integer;
		timeout_integer.QuadPart = -timeout.count();

		FILETIME file_timeout = std::bit_cast<FILETIME>(timeout_integer);
		SetThreadpoolWait(m_wait, handle, &file_timeout);
	}

	bool is_signaled() const
	{
		TP_WAIT_RESULT const result = m_result.load(std::memory_order_acquire);
		REQUIRE(result != WAIT_TIMEOUT);
		return result == WAIT_OBJECT_0;
	}

private:
	void non_static_callback(TP_WAIT_RESULT const result)
	{
		this->m_result.store(result, std::memory_order_release);
	}

	static void CALLBACK static_callback(
		PTP_CALLBACK_INSTANCE const instance,
		PVOID const context,
		PTP_WAIT const wait,
		TP_WAIT_RESULT const result)
	{
		static_cast<thread_pool_wait*>(context)->non_static_callback(result);
	}
};


TEST_CASE("Synchronous I/O with attached overlapped handle", "[windows][iocp]")
{
	auto const completion_port = create_completion_port(1).value();

	auto const event = create_event(
		/* auto_reset: */ true,
		/* is_initially_signaled: */ false).value();

	auto const [r, w] = nothrow::create_pipe(non_blocking).value();
	HANDLE const r_h = unwrap_handle(r.native().platform_handle);

	set_completion_information(r_h, completion_port.get(), nullptr).value();

	unsigned char r_buffer = 0;
	IO_STATUS_BLOCK io_status_block =
	{
		.Status = -1,
	};

	NTSTATUS const status = win32::NtReadFile(
		r_h,
		event.get(),
		/* ApcRoutine: */ nullptr,
		/* ApcContext: */ nullptr,
		&io_status_block,
		&r_buffer,
		sizeof(r_buffer),
		/* ByteOffset: */ nullptr,
		/* Key: */ 0);
	REQUIRE(status == STATUS_PENDING);

	auto future = test::spawn([&]() {
		return win32::NtWaitForSingleObject(
			event.get(),
			/* Alertable: */ false,
			kernel_timeout(std::chrono::seconds(1)));
	});
	std::this_thread::sleep_for(std::chrono::milliseconds(10));

	unsigned char w_buffer = 42;
	REQUIRE(w.write_some(as_write_buffer(&w_buffer, sizeof(w_buffer))).value() == 1);

	REQUIRE(future.get() == STATUS_SUCCESS);

	REQUIRE(io_status_block.Status == 0);
	REQUIRE(io_status_block.Information == 1);
	REQUIRE(r_buffer == 42);

	test::completion_storage<2> completions;
	size_t const completion_count = completions.remove(completion_port);
	REQUIRE(completion_count == 0);
}

TEST_CASE("I/O Completion Port becomes signaled when the queue has completions", "[windows][iocp]")
{
	auto const completion_port = create_completion_port(1).value();

	auto const [r, w] = nothrow::create_pipe(non_blocking).value();
	HANDLE const r_h = unwrap_handle(r.native().platform_handle);

	set_completion_information(r_h, completion_port.get(), nullptr).value();

	unsigned char r_buffer = 0;
	IO_STATUS_BLOCK io_status_block =
	{
		.Status = -1,
	};

	NTSTATUS const status = win32::NtReadFile(
		r_h,
		/* Event: */ NULL,
		/* ApcRoutine: */ nullptr,
		/* ApcContext: */ &io_status_block,
		&io_status_block,
		&r_buffer,
		sizeof(r_buffer),
		/* ByteOffset: */ nullptr,
		/* Key: */ 0);
	REQUIRE(status == STATUS_PENDING);

	bool const wait_multiple = GENERATE(0, 1);
	auto const wait = [&](deadline const deadline = deadline::instant())
	{
		NTSTATUS status = STATUS_PENDING;

		if (wait_multiple)
		{
			HANDLE const handle = completion_port.get();
			status = win32::NtWaitForMultipleObjects(
				1,
				&handle,
				WaitAnyObject,
				/* Alertable: */ false,
				kernel_timeout(deadline));
		}
		else
		{
			status = win32::NtWaitForSingleObject(
				completion_port.get(),
				/* Alertable: */ false,
				kernel_timeout(deadline));
		}

		if (status == STATUS_WAIT_0)
		{
			return true;
		}

		REQUIRE(status == STATUS_TIMEOUT);
		return false;
	};

	auto background_wait = test::spawn([&]()
	{
		REQUIRE(wait(std::chrono::milliseconds(100)) == false);
	});

	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	REQUIRE(wait(std::chrono::milliseconds(10)) == false);
	background_wait.get();

	(void)w.write_some(as_write_buffer("x", 1)).value();
	REQUIRE(wait() == true);

	test::completion_storage<2> completions;
	REQUIRE(completions.remove(completion_port) == 1);

	REQUIRE(wait() == false);
}

#if 0
TEST_CASE("I/O Completion Port can be waited using a thread pool wait", "[windows][iocp]")
{
#define allio_use_event 0

#if allio_use_event
	auto const event = win32::create_event().value();

	HANDLE const waited_handle = event.get();

	auto const set_signaled = [&]()
	{
		win32::signal_event(event.get()).value();
	};
#else
	auto const completion_port = create_completion_port(1).value();

	auto const [r, w] = nothrow::create_pipe(non_blocking).value();
	HANDLE const r_h = unwrap_handle(r.native().platform_handle);

	set_completion_information(r_h, completion_port.get(), nullptr).value();

	unsigned char r_buffer = 0;
	IO_STATUS_BLOCK io_status_block =
	{
		.Status = -1,
	};

	NTSTATUS const status = win32::NtReadFile(
		r_h,
		/* Event: */ NULL,
		/* ApcRoutine: */ nullptr,
		/* ApcContext: */ &io_status_block,
		&io_status_block,
		&r_buffer,
		sizeof(r_buffer),
		/* ByteOffset: */ nullptr,
		/* Key: */ 0);
	REQUIRE(status == STATUS_PENDING);

	HANDLE const waited_handle = completion_port.get();

	auto const set_signaled = [&]()
	{
		(void)w.write_some(as_write_buffer("x", 1)).value();
	};
#endif

	thread_pool_wait tp_wait;
	tp_wait.set(waited_handle, std::chrono::seconds(1));

	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	REQUIRE(tp_wait.is_signaled() == false);

	set_signaled();

	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	REQUIRE(tp_wait.is_signaled() == true);
}
#endif

#if 0
TEST_CASE("I/O Completion Port can be waited using a wait packet", "[windows][iocp]")
{
	auto const completion_port_1 = create_completion_port(1).value();
	auto const completion_port_2 = create_completion_port(1).value();

	auto const wait_packet = win32::create_wait_packet().value();

	int apc_context_object = 0;
	REQUIRE(!win32::associate_wait_packet(
		wait_packet.get(),
		completion_port_2.get(),
		completion_port_1.get(),
		/* key_context: */ nullptr,
		&apc_context_object,
		STATUS_SUCCESS,
		/* completion_information: */ 0).value());

	auto const [r, w] = nothrow::create_pipe(non_blocking).value();
	HANDLE const r_h = unwrap_handle(r.native().platform_handle);

	set_completion_information(r_h, completion_port_1.get(), nullptr).value();

	unsigned char r_buffer = 0;
	IO_STATUS_BLOCK io_status_block =
	{
		.Status = -1,
	};

	NTSTATUS const status = win32::NtReadFile(
		r_h,
		/* Event: */ NULL,
		/* ApcRoutine: */ nullptr,
		/* ApcContext: */ &io_status_block,
		&io_status_block,
		&r_buffer,
		sizeof(r_buffer),
		/* ByteOffset: */ nullptr,
		/* Key: */ 0);
	REQUIRE(status == STATUS_PENDING);
	
	auto const wait = [&]()
	{
		test::completion_storage<2> storage;
		if (size_t const count = storage.remove(completion_port_2, 2))
		{
			REQUIRE(count == 1);
			(void)storage.get(&apc_context_object);
		}
		return false;
	};

	REQUIRE(wait() == false);

	(void)w.write_some(as_write_buffer("x", 1)).value();

	REQUIRE(wait() == true);
}
#endif

} // namespace
