#include <allio/impl/win32/thread_event.hpp>

#include <allio/detail/unique_handle.hpp>

#include <vsm/atomic.hpp>
#include <vsm/out_resource.hpp>

#include <thread>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

vsm::result<thread_event> thread_event::get()
{
	auto const make_event = []() -> vsm::result<unique_handle>
	{
		vsm::result<unique_handle> r(vsm::result_value);

		NTSTATUS const status = NtCreateEvent(
			vsm::out_resource(*r),
			EVENT_ALL_ACCESS,
			/* ObjectAttributes: */ nullptr,
			SynchronizationEvent,
			/* InitialState: */ false);

		if (!NT_SUCCESS(status))
		{
			r = vsm::unexpected(static_cast<kernel_error>(status));
		}

		return r;
	};

	static thread_local auto const event = make_event();

	return event.transform([](unique_handle const& h)
	{
		return thread_event(h.get());
	});
}

[[nodiscard]] NTSTATUS thread_event::wait(deadline const deadline)
{
	vsm_assert(m_event != NULL); //PRECONDITION

	NTSTATUS const status = win32::NtWaitForSingleObject(
		m_event,
		/* Alertable: */ false,
		kernel_timeout(deadline));

	if (status == STATUS_WAIT_0)
	{
		m_reset = false;
	}

	return status;
}

[[nodiscard]] NTSTATUS thread_event::wait_for_io(
	HANDLE const handle,
	IO_STATUS_BLOCK& io_status_block,
	deadline const deadline)
{
	vsm_assert(m_event != NULL); //PRECONDITION

	NTSTATUS status;

	auto const read_status = [&]() -> NTSTATUS
	{
		return status = vsm::atomic_ref<NTSTATUS>(
			io_status_block.Status).load(std::memory_order_acquire);
	};

	if (read_status() == STATUS_PENDING)
	{
		NTSTATUS wait_status = wait(deadline);

		if (wait_status != STATUS_WAIT_0 && read_status() == STATUS_PENDING)
		{
			wait_status = NtCancelIoFileEx(
				handle,
				&io_status_block,
				&io_status_block);

			if (NT_SUCCESS(wait_status))
			{
				vsm_assert(read_status() != STATUS_PENDING);
			}
		}

		while (read_status() == STATUS_PENDING)
		{
			std::this_thread::yield();
		}
	}

	return status;
}

[[nodiscard]] NTSTATUS thread_event::wait_for_io(
	HANDLE const handle,
	OVERLAPPED& overlapped,
	deadline const deadline)
{
	return wait_for_io(
		handle,
		// This leads to UB. Hopefully the compiler doesn't notice.
		// At least we're using atomic operations which
		// compilers are generally hesitant to optimise.
		reinterpret_cast<IO_STATUS_BLOCK&>(overlapped),
		deadline);
}

void thread_event::reset_event()
{
	if (m_event != NULL && m_reset)
	{
		NTSTATUS const status = NtResetEvent(
			m_event,
			/* PreviousState: */ nullptr);

		vsm_assert(NT_SUCCESS(status));
	}
}