#include <allio/linux/detail/io_uring/event.hpp>

#include <allio/impl/linux/error.hpp>
#include <allio/impl/linux/event.hpp>
#include <allio/impl/linux/kernel/io_uring.hpp>
#include <allio/linux/io_uring_record_context.hpp>
#include <allio/linux/platform.hpp>

#include <poll.h>
#include <sys/eventfd.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

using M = io_uring_multiplexer;
using H = event_t;
using C = connector_t<M, H>;

using wait_t = event_t::wait_t;
using wait_s = operation_t<M, H, wait_t>;

static eventfd_t dummy_event_value;

static io_result2 _submit(M& m, H const& h, C const& c, wait_s& s)
{
	deadline relative_deadline;

	// Evaluate the stepped deadline.
	{
		auto const r = s.absolute_deadline.step();
		if (!r)
		{
			return r.error();
		}
		relative_deadline = *r;
	}

	io_uring_multiplexer::record_context ctx(m);

	auto const [fd, fd_flags] = ctx.get_fd(c, h.get_platform_handle());

	// Polling is required even in auto reset mode
	// because the event is opened in non-blocking mode.
	vsm_try(poll_sqe, ctx.push(
	{
		.opcode = IORING_OP_POLL_ADD,
		.flags = fd_flags,
		.fd = fd,
		.poll_events = POLLIN,
		.user_data = ctx.get_user_data(s.poll_slot),
	}));

	if (relative_deadline != deadline::never())
	{
		// Link the timeout to the poll SQE.
		vsm_try_void(ctx.link_timeout(s.timeout.set(relative_deadline)));
	}

	if (is_auto_reset(h))
	{
		// Link the previous SQE to this one.
		ctx.link_last(IOSQE_IO_LINK);

		// The read value is not actually needed for anything.
		// Just write the value into a global dummy buffer.
		vsm_try(read_sqe, ctx.push(
		{
			.opcode = IORING_OP_READ,
			.flags = fd_flags,
			.fd = fd,
			.addr = reinterpret_cast<uintptr_t>(&dummy_event_value),
			.len = sizeof(dummy_event_value),
			.user_data = ctx.get_user_data(s),
		}));

		// Successful poll CQE can be skipped when reading.
		ctx.set_cqe_skip_success(*poll_sqe);
		ctx.set_cqe_skip_success_emulation(s.poll_slot);

		// The read CQE can be skipped when it is canceled.
		ctx.set_cqe_skip_success_linked_emulation(*read_sqe);
	}

	vsm_try_void(ctx.commit());

	return std::nullopt;
}

io_result2 operation_impl<M, H, wait_t>::submit(M& m, H const& h, C const& c, wait_s& s)
{
	if (!h)
	{
		return error::handle_is_null;
	}

	// If the deadline is instant just check the event synchronously.
	if (s.args.deadline == deadline::instant())
	{
		return vsm::as_error_code(test_event(
			unwrap_handle(h.get_platform_handle()),
			is_auto_reset(h)));
	}

	s.poll_slot.bind(s);

	return _submit(m, h, c, s);
}

io_result2 operation_impl<M, H, wait_t>::notify(M& m, H const& h, C const& c, wait_s& s, io_status const status)
{
	auto const& cqe = M::unwrap_io_status(status);

	if (cqe.result < 0)
	{
		if (cqe.result == -EAGAIN)
		{
			// Only the read operation should ever return EAGAIN.
			// In any case though, retrying the operation is fine.
			vsm_assert(cqe.slot == nullptr);

			// The read operation is only submitted in auto reset mode.
			vsm_assert(is_auto_reset(h));

			// Someone else won the race to reset the event.
			// Retry by submitting both operations again.
			return _submit(m, h, c, s);
		}

		return static_cast<system_error>(-cqe.result);
	}

	// Only one successful CQE is posted, either for the read or the poll.
	vsm_assert(cqe.slot == (is_auto_reset(h) ? nullptr : &s.poll_slot));

	return std::error_code();

#if 0
	if (cqe.slot == &s.poll_slot)
	{
		if (cqe.result < 0)
		{
			return static_cast<system_error>(-cqe.result);
		}

		vsm_assert(cqe.result == POLLIN);

		if (!is_auto_reset(h))
		{
			return std::error_code();
		}
	}
	else
	{
		// The read operation is only submitted in auto reset mode.
		vsm_assert(is_auto_reset(h));

		if (cqe.result < 0)
		{
			if (cqe.result == -EAGAIN)
			{
				// Someone else won the race to reset the event.
				// Submit the operation again to retry.
				return _submit(m, h, c, s);
			}

			return static_cast<system_error>(-cqe.result);
		}

		return std::error_code();
	}

	return std::nullopt;
#endif

#if 0
	if (cqe.result < 0)
	{
		if (auto_reset && cqe.result == -EAGAIN)
		{
			// Only the read operation should ever return EAGAIN.
			// In any case though, retrying the operation is fine.
			vsm_assert(cqe.slot == nullptr);

			return _submit(m, h, c, s);
		}

		return static_cast<system_error>(-cqe.result);
	}

	if (cqe.slot == &s.poll_slot)
	{
		vsm_assert(cqe.result == POLLIN);

		if (auto_reset)
		{
			// In auto reset mode the poll completion is ignored.
			// The subsequent read operation is still pending.
			return std::nullopt;
		}
	}

	vsm_assert(cqe.slot == nullptr);
	return std::error_code();
#endif
}

void operation_impl<M, H, wait_t>::cancel(M& m, H const& h, C const& c, S& s)
{
	//TODO: Cancel
	//m.cancel_io(s.poll_slot);
}
