#include <allio/impl/win32/thread_event.hpp>

#include <allio/impl/win32/event.hpp>

#include <vsm/atomic.hpp>

#include <thread>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

static NTSTATUS read_status(IO_STATUS_BLOCK& io_status_block)
{
	return vsm::atomic_ref<NTSTATUS>(io_status_block.Status).load(std::memory_order_acquire);
}

static void cancel_io_and_wait(
	HANDLE const handle,
	IO_STATUS_BLOCK& io_status_block,
	NTSTATUS const status_on_cancellation)
{
	IO_STATUS_BLOCK io_status_block_cancel;
	NTSTATUS const status = NtCancelIoFileEx(
		handle,
		&io_status_block,
		&io_status_block_cancel);

	if (!NT_SUCCESS(status))
	{
		unrecoverable_error(static_cast<kernel_error>(status));
	}

	while (read_status(io_status_block) == STATUS_PENDING)
	{
		std::this_thread::yield();
	}

	if (NT_SUCCESS(status) && io_status_block.Status == STATUS_CANCELLED)
	{
		io_status_block.Status = status_on_cancellation;
	}
}

vsm::result<thread_event> thread_event::get()
{
	static thread_local auto const event = create_event();

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
	io_status_block_t& io_status_block,
	deadline const deadline)
{
	vsm_assert(m_event != NULL); //PRECONDITION

	NTSTATUS const status = wait(deadline);

	if (!NT_SUCCESS(status))
	{
		unrecoverable_error(static_cast<kernel_error>(status));
	}

	if (status == STATUS_TIMEOUT)
	{
		cancel_io_and_wait(handle, io_status_block, STATUS_TIMEOUT);
	}
	else
	{
		vsm_assert(status == STATUS_WAIT_0);
	}

	vsm_assert(io_status_block.Status != STATUS_PENDING);

	return io_status_block.Status;
}

[[nodiscard]] NTSTATUS thread_event::wait_for_io(
	HANDLE const handle,
	overlapped_t& overlapped,
	deadline const deadline)
{
	return wait_for_io(
		handle,
		// This is technically UB. Hopefully the compiler doesn't notice. At least we're using
		// atomic operations which compilers are generally hesitant to optimise.
		reinterpret_cast<io_status_block_t&>(overlapped),
		deadline);
}

void thread_event::reset_event()
{
	if (m_event != NULL && m_reset)
	{
		NTSTATUS const status = NtResetEvent(
			m_event,
			/* PreviousState: */ nullptr);

		if (NT_SUCCESS(status))
		{
			m_reset = false;
		}
		else
		{
			//TODO: Call unrecoverable_error instead.
			//      Rename to unhandled_error and add a parameter describing severity.
			std::terminate();
		}
	}
}