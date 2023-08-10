#include <allio/impl/linux/event_handle.hpp>

#include <allio/impl/linux/error.hpp>
#include <allio/impl/linux/platform_handle.hpp>
#include <allio/linux/timeout.hpp>

#include <sys/eventfd.h>
#include <poll.h>

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

		// If the counter is already full, EAGAIN is returned.
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

	if (args.auto_reset)
	{
		h_flags |= event_handle::flags::auto_reset;
	}

	int const event = eventfd(
		args.signal ? 1 : 0,
		EFD_CLOEXEC | EFD_NONBLOCK | EFD_SEMAPHORE);

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

	// Poll the event fd for a non-zero value without resetting it.
	{
		pollfd fd =
		{
			.fd = event,
			.events = POLLIN,
		};

		int const result = ppoll(
			&fd,
			1,
			kernel_timeout<timespec>(args.deadline),
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
		vsm_try(reset, reset_event(event));

		// After just polling, the counter must be non-zero.
		// The only case where it would not be is due to a race with reset.
		vsm_assert(reset);
	}

	return {};
}
