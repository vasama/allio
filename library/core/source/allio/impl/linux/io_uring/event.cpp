#include <allio/linux/detail/io_uring/event.hpp>

#include <allio/impl/error_encoding.hpp>
#include <allio/impl/linux/error.hpp>
#include <allio/impl/linux/handles/event.hpp>
#include <allio/impl/linux/io_uring.hpp>
#include <allio/linux/io_uring_record_context.hpp>

#include <poll.h>
#include <sys/eventfd.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

using M = io_uring_multiplexer;
using H = native_handle<event_t>;
using C = async_connector_t<M, event_t>;

using wait_t = event_t::wait_t;
using wait_s = async_operation_t<M, event_t, wait_t>;
using wait_a = io_parameters_t<event_t, wait_t>;

static constexpr uintptr_t wait_for_next_cqe                = 1;
static constexpr uintptr_t use_previous_error               = 2;

static io_result<void> _submit(
	M& m,
	H const& h,
	C const& c,
	wait_s& s,
	wait_a const& a,
	io_handler<M>& handler)
{
	// Evaluate the stepped deadline.
	vsm_try(relative_deadline, s.absolute_deadline.step());

	io_uring_record_context ctx(m, relative_deadline);
	auto const [fd, fd_flags] = ctx.get_fd(c, h.platform_handle);

	bool const auto_reset = is_auto_reset(h);
	bool const double_cqe = auto_reset && !ctx.has_skip_cqe_on_success();

	// Polling is required even in auto reset mode because the event is opened in non-blocking mode.
	vsm_try_ptr(poll_sqe, ctx.push());

	poll_sqe =
	{
		.opcode = IORING_OP_POLL_ADD,
		.flags = fd_flags,
		.fd = fd,
		.poll_events = POLLIN,
		.user_data = ctx.encode_user_data(s, double_cqe ? wait_for_next_cqe : 0),
	};

	if (relative_deadline != deadline::never())
	{
		// Link the timeout to the poll SQE.
		vsm_try_void(ctx.link_timeout(s.timeout.set(relative_deadline)));
	}

	if (auto_reset)
	{
		// The read value is not actually needed for anything. Just read the value into a global
		// dummy buffer.
		static constinit eventfd_t dummy_event_value = 0;

		// Successful poll CQE may be skipped when reading.
		ctx.may_skip_cqe_on_success(poll_sqe);

		// Link the previous SQE to this one.
		ctx.link_last(IOSQE_IO_LINK);

		vsm_try_ptr(read_sqe, ctx.push());

		read_sqe =
		{
			.opcode = IORING_OP_READ,
			.flags = fd_flags,
			.fd = fd,
			.addr = reinterpret_cast<uintptr_t>(&dummy_event_value),
			.len = sizeof(dummy_event_value),
			.user_data = ctx.encode_user_data(s, double_cqe ? use_previous_error : 0),
		};
	}

	ctx.commit();

	return vsm::unexpected(io_notify_status::submitted);
}

io_result<void> wait_s::submit(
	M& m,
	H const& h,
	C const& c,
	wait_s& s,
	wait_a const& a,
	io_handler<M>& handler)
{
	// If the deadline is instant just check the event synchronously.
	if (a.deadline == deadline::instant())
	{
		// TODO: Is this reasonable? We should probably still use io_uring for this. At least if it
		//       is known that io_uring_enter will not need to be called. If a syscall is required
		//       either way, then perhaps testing synchronously is fine?

		return linux::test_event(unwrap_handle(h.platform_handle), is_auto_reset(h));
	}

	s.set_handler(handler);
	s.absolute_deadline = a.deadline;
	s.poll_slot.bind(s);

	return _submit(m, h, c, s, a, handler);
}

io_result<void> wait_s::notify(
	M& m,
	H const& h,
	C const& c,
	wait_s& s,
	wait_a const& a,
	io_handler<M>& handler,
	M::io_status_type const status)
{
	if (status.user_tag & wait_for_next_cqe)
	{
		s.previous_error = status.result < 0 ? -status.result : 0;
		return vsm::unexpected(io_notify_status::submitted);
	}

	if (status.result < 0)
	{
		switch (status.result)
		{
		case -EAGAIN:
			// The read operation is only submitted in auto reset mode.
			vsm_assert(is_auto_reset(h));

			if (s.is_cancel_requested())
			{
				return vsm::unexpected(io_error_code(
					io_notify_status::cancelled,
					allio_error(error::operation_canceled)));
			}

			// Someone else won the race to reset the event. Retry by submitting both operations
			// again.
			return _submit(m, h, c, s, a, handler);

		case -ECANCELED:
			if (status.user_tag & use_previous_error)
			{
				if (s.previous_error != 0)
				{
					vsm::unexpected(allio_error(static_cast<system_error>(s.previous_error)));
				}
			}
			else
			{
				return vsm::unexpected(io_error_code(
					io_notify_status::cancelled,
					allio_error(static_cast<system_error>(ECANCELED))));
			}
			break;

		default:
			return vsm::unexpected(allio_error(static_cast<system_error>(-status.result)));
		}
	}

	return {};
}

void wait_s::cancel(M& m, H const&, C const&, S& s)
{
	(void)m.cancel_io(s, s.poll_slot);
}
