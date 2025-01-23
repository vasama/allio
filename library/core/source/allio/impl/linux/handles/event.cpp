#include <allio/impl/linux/handles/event.hpp>

#include <allio/impl/linux/error.hpp>
#include <allio/impl/linux/eventfd.hpp>
#include <allio/impl/linux/poll.hpp>
#include <allio/step_deadline.hpp>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

vsm::result<void> linux::test_event(int const fd, bool const auto_reset)
{
	if (auto_reset)
	{
		vsm_try(was_non_zero, eventfd_reset(fd));

		if (was_non_zero)
		{
			return {};
		}

		return vsm::unexpected(allio_error(error::operation_timed_out));
	}
	else
	{
		return vsm::discard_value(linux::poll(fd, POLLIN, deadline::instant()));
	}
}


vsm::result<void> event_t::create(
	native_handle<event_t>& h,
	io_parameters_t<event_t, create_t> const& a)
{
	handle_flags flags = flags::none;
	if (vsm::any_flags(a.options, event_options::auto_reset))
	{
		flags |= flags::auto_reset;
	}

	int eventfd_flags = EFD_NONBLOCK;
	if (vsm::no_flags(a.flags, io_flags::create_inheritable))
	{
		eventfd_flags |= EFD_CLOEXEC;
	}

	bool const initially_signaled = vsm::any_flags(
		a.options,
		event_options::initially_signaled);

	vsm_try(fd, linux::eventfd(
		eventfd_flags,
		static_cast<unsigned>(initially_signaled ? 1 : 0)));

	h = native_handle<event_t>
	{
		native_handle<platform_object_t>
		{
			native_handle<object_t>
			{
				flags::not_null | flags,
			},
			wrap_handle(fd.release()),
		},
	};

	return {};
}

vsm::result<void> event_t::signal(
	native_handle<event_t> const& h,
	io_parameters_t<event_t, signal_t> const& a)
{
	return eventfd_signal(unwrap_handle(h.platform_handle));
}

vsm::result<void> event_t::reset(
	native_handle<event_t> const& h,
	io_parameters_t<event_t, reset_t> const& a)
{
	// It doesn't matter whether the counter was already zero, as long as it is now zero. Thus the
	// value can be discarded.
	return vsm::discard_value(
		eventfd_reset(unwrap_handle(h.platform_handle))
	);
}

vsm::result<void> event_t::wait(
	native_handle<event_t> const& h,
	io_parameters_t<event_t, wait_t> const& a)
{
	int const fd = unwrap_handle(h.platform_handle);
	bool const auto_reset = is_auto_reset(h);

	if (a.deadline == deadline::instant())
	{
		return test_event(fd, auto_reset);
	}

	// The deadline must be made absolute and stepped in each iteration.
	step_deadline absolute_deadline(a.deadline);

	while (true)
	{
		// Evaluate the stepped deadline. If the user provided a relative deadline a timeout error
		// may be returned here only after the first iteration. In the case of an absolute deadline,
		// poll may never get called.
		vsm_try(relative_deadline, absolute_deadline.step());

		// Poll the event fd for a non-zero value without resetting it. Ideally in auto reset mode
		// both wait and reset would happen using the same syscall, but there are two problems:
		// 1. read and its ilk don't support timeouts like poll does.
		// 2. The event must be opened in non-blocking mode to allow non-blocking reset using write
		//    even in the unlikely case of a maxed out counter value. There is also no way to open
		//    the event in both blocking and non-blocking modes at the same time.
		vsm_try_discard(linux::poll(fd, POLLIN, relative_deadline));

		if (auto_reset)
		{
			vsm_try(was_non_zero, eventfd_reset(fd));

			// It is possible that another thread (possibly in another process) reset the event
			// object between polling and reading. The auto reset mode guarantees that only a single
			// waiter will observe the signaled state.
			if (!was_non_zero)
			{
				continue;
			}
		}

		return {};
	}
}
