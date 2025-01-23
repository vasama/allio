#pragma once

#include <allio/detail/unique_handle.hpp>
#include <allio/impl/error_encoding.hpp>
#include <allio/impl/linux/error.hpp>

#include <vsm/assert.h>
#include <vsm/lazy.hpp>
#include <vsm/result.hpp>

#include <limits>

#include <sys/eventfd.h>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

inline vsm::result<eventfd_t> eventfd_read(int const fd)
{
	eventfd_t value;
	if (::eventfd_read(fd, &value) == -1)
	{
		return vsm::unexpected(allio_error(get_last_error()));
	}
	return value;
}

inline vsm::result<void> eventfd_write(int const fd, eventfd_t const value)
{
	if (::eventfd_write(fd, value) == -1)
	{
		return vsm::unexpected(allio_error(get_last_error()));
	}
	return {};
}

inline vsm::result<void> eventfd_signal(int const fd)
{
	int const r = ::eventfd_write(
		fd,
		/* value: */ 1);

	if (r == -1)
	{
		// If the counter is already full, EAGAIN is returned. This case is extremely unlikely, as
		// it would require signaling the event object 2^64-1 times. However this case is also not
		// problematic. The event object remains signaled as long as the counter is non-zero.
		if (int const e = errno; e != EAGAIN)
		{
			return vsm::unexpected(allio_error(static_cast<system_error>(e)));
		}
	}

	return {};
}

inline vsm::result<bool> eventfd_reset(int const fd)
{
	eventfd_t value;

	int const r = ::eventfd_read(
		fd,
		&value);

	if (r == -1)
	{
		// If the counter is already zero, EAGAIN is returned.
		if (int const e = errno; e != EAGAIN)
		{
			return vsm::unexpected(allio_error(static_cast<system_error>(e)));
		}

		return false;
	}

	vsm_assert(value != 0);

	return true;
}

inline vsm::result<detail::unique_handle> eventfd(
	int const flags,
	unsigned const initial_value = 0)
{
	vsm_assert((flags & ~(EFD_CLOEXEC | EFD_NONBLOCK | EFD_SEMAPHORE)) == 0); //PRECONDITION

	int const fd = ::eventfd(initial_value, flags);

	if (fd == -1)
	{
		return vsm::unexpected(allio_error(get_last_error()));
	}

	return vsm_lazy(detail::unique_handle(fd));
}

inline vsm::result<detail::unique_handle> eventfd(
	int const flags,
	eventfd_t const initial_value)
{
	static_assert(std::is_unsigned_v<eventfd_t>);
	vsm_assert(initial_value <= std::numeric_limits<eventfd_t>::max() - 1); //PRECONDITION

	if (initial_value < static_cast<eventfd_t>(std::numeric_limits<unsigned>::max()))
	{
		return eventfd(flags, static_cast<unsigned>(initial_value));
	}

	vsm_try(fd, eventfd(flags, static_cast<unsigned>(0)));
	vsm_try_void(eventfd_write(fd.get(), initial_value));
	return fd;
}

} // namespace allio::linux

#include <allio/linux/detail/undef.i>
