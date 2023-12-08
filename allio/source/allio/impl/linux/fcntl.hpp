#pragma once

#include <allio/impl/linux/error.hpp>
#include <allio/linux/detail/unique_fd.hpp>

#include <vsm/lazy.hpp>

#include <fcntl.h>
#include <unistd.h>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

inline vsm::result<int> fcntl(int const fd, int const cmd, auto&&... args)
{
	int const r = ::fcntl(fd, cmd, vsm_forward(args)...);

	if (r == -1)
	{
		return vsm::unexpected(get_last_error());
	}

	return r;
}

inline vsm::result<detail::unique_fd> duplicate_fd(int const old_fd, int new_fd, int const flags)
{
	// O_CLOEXEC is the only valid flag.
	vsm_assert((flags & ~O_CLOEXEC) == 0);

	if (new_fd == -1)
	{
		int const cmd = flags & O_CLOEXEC
			? F_DUPFD_CLOEXEC
			: F_DUPFD;

		new_fd = ::fcntl(old_fd, cmd, /* lowest_new_fd: */ 0);
	}
	else
	{
		new_fd = ::dup3(old_fd, new_fd, flags);
	}

	if (new_fd == -1)
	{
		return vsm::unexpected(get_last_error());
	}

	return vsm_lazy(detail::unique_fd(new_fd));
}

} // namespace allio::linux

#include <allio/linux/detail/undef.i>
