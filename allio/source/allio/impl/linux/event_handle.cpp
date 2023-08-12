#include <allio/impl/linux/event_handle.hpp>

#include <allio/impl/linux/error.hpp>
#include <allio/impl/linux/platform_handle.hpp>
#include <allio/linux/timeout.hpp>
#include <allio/step_deadline.hpp>

#include <poll.h>
#include <sys/eventfd.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

vsm::result<bool> linux::reset_event(int const fd)
{
	eventfd_t value;

	int const r = eventfd_read(
		fd,
		&value);

	if (r == -1)
	{
		int const e = errno;

		// If the counter is already zero, EAGAIN is returned.
		if (e != EAGAIN)
		{
			return vsm::unexpected(static_cast<system_error>(e));
		}

		return false;
	}

	vsm_assert(value != 0);

	return true;
}


vsm::result<void> event_handle_base::signal() const
{
	if (!*this)
	{
		return vsm::unexpected(error::handle_is_null);
	}

	int const r = eventfd_write(
		unwrap_handle(get_platform_handle()),
		1);

	if (r == -1)
	{
		int const e = errno;

		// If the counter is already full, EAGAIN is returned. This case is extremely unlikely,
		// as it would require signaling the event object 2^64-1 times. However this case is also
		// not problematic. The event object remains signaled as long as the counter is non-zero.
		if (e != EAGAIN)
		{
			return vsm::unexpected(static_cast<system_error>(e));
		}
	}

	return {};
}

vsm::result<void> event_handle_base::reset() const
{
	if (!*this)
	{
		return vsm::unexpected(error::handle_is_null);
	}

	// It doesn't matter whether the counter was already zero,
	// as long as it is now zero. Thus the value can be discarded.
	return vsm::discard_value(
		reset_event(unwrap_handle(get_platform_handle()))
	);
}


vsm::result<void> event_handle_base::sync_impl(io::parameters_with_result<io::event_create> const& args)
{
	event_handle& h = *args.handle;

	if (h)
	{
		return vsm::unexpected(error::handle_is_not_null);
	}

	handle_flags h_flags = {};

	if (args.reset_mode == event_handle::auto_reset)
	{
		h_flags |= event_handle::flags::auto_reset;
	}

	int const event = eventfd(
		args.signal ? 1 : 0,
		EFD_CLOEXEC | EFD_NONBLOCK);

	if (event == -1)
	{
		return vsm::unexpected(get_last_error());
	}

	return initialize_platform_handle(h, unique_fd(event),
		[&](native_platform_handle const handle)
		{
			return platform_handle::native_handle_type
			{
				handle::native_handle_type
				{
					h_flags | handle::flags::not_null,
				},
				handle,
			};
		}
	);
}

vsm::result<void> event_handle_base::sync_impl(io::parameters_with_result<io::event_wait> const& args)
{
	event_handle const& h = *args.handle;

	if (!h)
	{
		return vsm::unexpected(error::handle_is_not_null);
	}

	int const event = unwrap_handle(h.get_platform_handle());

	// The deadline must be made absolute and stepped in each iteration.
	step_deadline absolute_deadline(args.deadline);

	while (true)
	{
		// Poll the event fd for a non-zero value without resetting it.
		// Ideally in auto reset mode both wait and reset would happen
		// using the same syscall, but there are two problems:
		// * read and its ilk don't support timeouts like poll does.
		// * The event must be opened in non-blocking mode to allow non-blocking reset
		//   using write even in the unlikely case of a maxed out counter value.
		//   Opening the event twice in both blocking and non-blocking modes might work,
		//   but that is also not a particularly satisfying solution.
		{
			// Evaluate the stepped deadline. If the user provided a relative deadline
			// a timeout error may be returned here only after the first iteration.
			// In the case of an absolute deadline, poll may never get called.
			vsm_try(relative_deadline, absolute_deadline.step());

			pollfd fd =
			{
				.fd = event,
				.events = POLLIN,
			};

			int const result = ppoll(
				&fd,
				1,
				kernel_timeout<timespec>(relative_deadline),
				nullptr);

			if (result == -1)
			{
				return vsm::unexpected(get_last_error());
			}

			if (result == 0)
			{
				return vsm::unexpected(error::async_operation_timed_out);
			}

			vsm_assert(fd.revents == POLLIN);
		}

		if (h.get_flags()[event_handle::flags::auto_reset])
		{
			vsm_try(was_non_zero, reset_event(event));

			// It is possible that another thread (possibly in another process)
			// reset the event object between polling and reading. The auto reset mode
			// guarantees that only a single waiter will observe the signaled state.
			if (was_non_zero)
			{
				return {};
			}
		}
	}
}
