#pragma once

#include <allio/impl/linux/error.hpp>
#include <allio/linux/detail/unique_fd.hpp>

#include <fcntl.h>
#include <unistd.h>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

inline vsm::result<unique_fd> dup(int const old_fd, int new_fd, int const flags)
{
	vsm_assert((flags & ~O_CLOEXEC) == 0);

	if (new_fd == -1)
	{
		int const cmd = flags & O_CLOEXEC
			? F_DUPFD_CLOEXEC
			: F_DUPFD;

		new_fd = ::fcntl(old_fd, cmd, 0);
	}
	else
	{
		new_fd = ::dup3(old_fd, new_fd, flags);
	}

	if (new_fd == -1)
	{
		return vsm::unexpected(get_last_error());
	}

	return vsm::result<unique_fd>(result_value, new_fd);
}

} // namespace allio::linux

#include <allio/linux/detail/undef.i>
