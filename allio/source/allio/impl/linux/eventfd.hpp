#pragma once

#include <allio/impl/linux/error.hpp>
#include <allio/linux/detail/unique_fd.hpp>

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
		return vsm::unexpected(get_last_error());
	}
	return value;
}

inline vsm::result<void> eventfd_write(int const fd, eventfd_t const value)
{
	if (::eventfd_write(fd, value) == -1)
	{
		return vsm::unexpected(get_last_error());
	}
	return {};
}

inline vsm::result<detail::unique_fd> eventfd(
	int const flags,
	int const initial_value)
{
	vsm_assert((flags & ~(EFD_CLOEXEC | EFD_NONBLOCK | EFD_SEMAPHORE)) == 0); //PRECONDITION

	int const fd = ::eventfd(initial_value, flags);

	if (fd == -1)
	{
		return vsm::unexpected(get_last_error());
	}

	return vsm_lazy(detail::unique_fd(fd));
}

inline vsm::result<detail::unique_fd> eventfd(
	int const flags,
	eventfd_t const initial_value = 0)
{
	vsm_assert(initial_value <= std::numeric_limits<eventfd_t>::max() - 1); //PRECONDITION

	if (initial_value < static_cast<eventfd_t>(std::numeric_limits<int>::max()))
	{
		return eventfd(flags, static_cast<int>(initial_value));
	}

	vsm_try(fd, eventfd(flags, 0));
	vsm_try_void(eventfd_write(fd.get(), initial_value));
	return fd;
}

} // namespace allio::linux

#include <allio/linux/detail/undef.i>
