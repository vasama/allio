#pragma once

#include <allio/detail/unique_handle.hpp>
#include <allio/impl/linux/error.hpp>

#include <vsm/lazy.hpp>

#include <sys/epoll.h>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

inline vsm::result<detail::unique_handle> epoll_create()
{
	int const fd = epoll_create1(EPOLL_CLOEXEC);
	if (fd == -1)
	{
		return vsm::unexpected(get_last_error());
	}
	return vsm_lazy(detail::unique_handle(fd));
}

inline vsm::result<void> epoll_ctl(
	int const epoll_fd,
	int const operation,
	int const fd,
	epoll_event* const event)
{
	if (::epoll_ctl(epoll_fd, operation, fd, event) == -1)
	{
		return vsm::unexpected(get_last_error());
	}
	return {};
}

} // namespace allio::linux

#include <allio/linux/detail/undef.i>
