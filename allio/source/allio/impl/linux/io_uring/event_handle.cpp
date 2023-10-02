#include <allio/linux/io_uring/event_handle.hpp>

#include <allio/impl/linux/event_handle.hpp>
#include <allio/impl/linux/io_uring/io_uring.hpp>

#include <poll.h>
#include <sys/eventfd.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

using M = io_uring_multiplexer;
using H = _event_handle;
using C = connector_t<M, H>;

using wait_t = _event_handle::wait_t;
using wait_s = operation_t<M, H, wait_t>;

static vsm::result<io_result> _submit(M& m, H const& h, C const& c, wait_s& s)
{
	deadline relative_deadline;
	{
		auto const r = s.absolute_deadline.step();
		if (!r)
		{
			return r.error();
		}
		relative_deadline = *r;
	}

	return m.record_io([&](io_uring_multiplexer::submission_context& ctx)
	{
		bool const auto_reset = is_auto_reset(h);

		if (relative_deadline != deadline::never())
		{
			vsm_try_void(c.push_linked_timeout(s.timeout.set(relative_deadline)));
		}

		vsm_try_void(c.push(s.poll_data, [&](io_uring_sqe& sqe)
		{
			sqe.opcode = IORING_OP_POLL_ADD;
			sqe.fd = unwrap_handle(h.get_platform_handle());
			sqe.poll_events = POLLIN;

			if (auto_reset)
			{
				sqe.flags = IOSQE_IO_LINK | IOSQE_CQE_SKIP_SUCCESS;
			}
		}));

		if (auto_reset)
		{
			vsm_try_void(c.push(s, [&](io_uring_sqe& sqe)
			{
				sqe.opcode = IORING_OP_READ;
				sqe.fd = unwrap_handle(h.get_platform_handle());
				sqe.addr = reinterpret_cast<uintptr_t>(&s.event_value);
				sqe.len = sizeof(s.event_value);
			}));
		}

		return {};
	});
}

vsm::result<io_result> operation_impl<M, H, wait_t>::submit(M& m, H const& h, C const& c, wait_s& s)
{
	if (!h)
	{
		return error::handle_is_null;
	}

	// If the deadline is instant just check the event synchronously.
	if (args.deadline == deadline::instant())
	{
		return vsm::as_error_code(check_event(
			unwrap_handle(h.get_platform_handle()),
			is_auto_reset(h)));
	}

	return _submit(m, h, c, s);
}

vsm::result<io_result> operation_impl<M, H, wait_t>::notify(M& m, H const& h, C const& c, wait_s& s, io_status const status)
{
	bool const auto_reset = h.get_flags()[_event_handle::flags::auto_reset];

	auto const& cqe = M::unwrap_io_status(status);

	if (cqe.result < 0)
	{
		if (cqe.result == -EAGAIN)
		{
			// The poll operation should never return EAGAIN.
			// If it does though, restarting the operation would be fine.
			vsm_assert(cqe.slot == nullptr);

			// Someone else reset the event. Restart the operation to try again.
			if (auto_reset)
			{
				return _submit(m, h, c, s);
			}
		}

		return static_cast<system_error>(-cqe.result);
	}

	// Completion of the poll operation is ignored in auto reset mode.
	if (auto_reset && cqe.slot != nullptr)
	{
		vsm_assert(cqe.slot == &s.poll_data);
		vsm_assert(cqe.result == POLLIN);
		return std::nullopt;
	}

	return std::error_code();
}

void operation_impl<M, H, wait_t>::cancel(M& m, H const& h, C const& c, S& s)
{
	//TODO: Cancel.
}
