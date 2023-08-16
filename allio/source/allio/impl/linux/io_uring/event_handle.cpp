#include <allio/linux/io_uring/event_handle.hpp>

#include <linux/io_uring.h>
#include <poll.h>
#include <sys/eventfd.h>

#include <allio/linux/detail/undef.i>

namespace allio {

using namespace linux;

template<>
struct async_handle_impl<io_uring_multiplexer, detail::event_handle_impl>
{
	static vsm::result<void> attach(event_handle const& h, context& c) noexcept
	{
		vsm_try(blocking_event, dup(
			unwrap_handle(h.get_platform_handle()),
			-1,
			O_CLOEXEC));

		vsm_try_void(fcntl())
	}
};

template<>
struct async_operation_impl<io_uring_multiplexer, detail::event_handle_impl, event_handle::wait_tag>
{
	using storage = async_operation_storage<io_uring_multiplexer, detail::event_handle_impl, event_handle::wait_tag>;

	static vsm::result<void> submit(io_uring_multiplexer& m, storage& s) noexcept
	{
		return m.record_io([&](io_uring_multiplexer::submission_context& c) -> async_result<void>
		{
			deadline const deadline = s.args.deadline;

			if (!deadline.is_trivial())
			{
				vsm_try_void(c.push_linked_timeout(s.timeout.set(deadline)));
			}

			if (h.get_flags()[event_handle::flags::auto_reset])
			{
				// Read from the blocking event handle.
				return c.push(s, [&](io_uring_sqe& sqe)
				{
					sqe.opcode = IORING_OP_READ;
					// Select between non-blocking and blocking fd depending on deadline.
					sqe.fd = deadline == deadline::instant()
						? unwrap_handle(s.handle->get_platform_handle())
						: s.blocking_event.get();
					sqe.addr = reinterpret_cast<uintptr_t>(&s.event_value);
					sqe.len = sizeof(s.event_value);
				});
			}
			else
			{
				// Poll the non-blocking event handle.
				return c.push(s, [&](io_uring_sqe& sqe)
				{
					sqe.opcode = IORING_OP_POLL_ADD;
					sqe.fd = unwrap_handle(s.handle->get_platform_handle());
					sqe.poll_events = POLLIN;
				});
			}
		});
	}

	static vsm::result<void> cancel(io_uring_multiplexer& m, storage& s) noexcept
	{
		return m.cancel(s);
	}
};

} // namespace allio
