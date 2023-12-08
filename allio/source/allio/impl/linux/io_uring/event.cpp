#include <allio/linux/detail/io_uring/event.hpp>

#include <allio/impl/linux/error.hpp>
#include <allio/impl/linux/handles/event.hpp>
#include <allio/impl/linux/io_uring.hpp>
#include <allio/linux/io_uring_record_context.hpp>
#include <allio/linux/platform.hpp>

#include <poll.h>
#include <sys/eventfd.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

using M = io_uring_multiplexer;
using H = event_t::native_type;
using C = async_connector_t<M, event_t>;

using wait_t = event_io::wait_t;
using wait_s = async_operation_t<M, event_t, wait_t>;
using wait_a = io_parameters_t<event_t, wait_t>;

static eventfd_t dummy_event_value;

static io_result<void> _submit(M& m, H const& h, C const& c, wait_s& s, io_handler<M>& handler)
{
	deadline relative_deadline;

	// Evaluate the stepped deadline.
	vsm_try_assign(relative_deadline, s.absolute_deadline.step());

	io_uring_multiplexer::record_context ctx(m);

	auto const [fd, fd_flags] = ctx.get_fd(c, h.platform_handle);

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
			.user_data = ctx.get_user_data(handler),
		}));

		// Successful poll CQE can be skipped when reading.
		ctx.set_cqe_skip_success(*poll_sqe);
		ctx.set_cqe_skip_success_emulation(s.poll_slot);

		// The read CQE can be skipped when it is cancelled.
		ctx.set_cqe_skip_success_linked_emulation(*read_sqe);
	}

	vsm_try_void(ctx.commit());

	return io_pending(error::async_operation_pending);
}

io_result<void> wait_s::submit(M& m, H const& h, C const& c, wait_s& s, wait_a const& a, io_handler<M>& handler)
{
	// If the deadline is instant just check the event synchronously.
	if (a.deadline == deadline::instant())
	{
		return linux::test_event(
			unwrap_handle(h.platform_handle),
			is_auto_reset(h));
	}

	s.handler = &handler;
	s.poll_slot.bind(handler);
	s.absolute_deadline = a.deadline;

	return _submit(m, h, c, s, handler);
}

io_result<void> wait_s::notify(M& m, H const& h, C const& c, wait_s& s, wait_a const&, M::io_status_type const status)
{
	if (status.result < 0)
	{
		switch (status.result)
		{
		case -EAGAIN:
			// Only the read operation should ever return EAGAIN,
			// through retrying the operation is fine in either case.
			vsm_assert(status.slot == nullptr);

			// The read operation is only submitted in auto reset mode.
			vsm_assert(is_auto_reset(h));

			// Someone else won the race to reset the event.
			// Retry by submitting both operations again.
			return _submit(m, h, c, s, *s.handler);

		case -ECANCELED:
			return io_cancelled(static_cast<system_error>(ECANCELED));
		}

		return vsm::unexpected(static_cast<system_error>(-status.result));
	}

	// Only one successful CQE is posted, either for the read or the poll.
	vsm_assert(status.slot == (is_auto_reset(h) ? nullptr : &s.poll_slot));

	return {};
}

void wait_s::cancel(M& m, H const&, C const&, S& s)
{
	m.cancel_io(s.poll_slot);
}
