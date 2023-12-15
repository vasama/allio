#pragma once

#include <allio/detail/unique_handle.hpp>
#include <allio/impl/linux/error.hpp>

#include <vsm/lazy.hpp>

#include <signal.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

inline vsm::result<detail::unique_handle> pidfd_open(pid_t const pid, unsigned const flags)
{
	int const fd = syscall(SYS_pidfd_open, pid, flags);

	if (fd == -1)
	{
		return vsm::unexpected(get_last_error());
	}

	return vsm_lazy(detail::unique_handle(fd));
}

inline int pidfd_send_signal(int const fd, int const sig, siginfo_t* const info, unsigned const flags)
{
	return syscall(SYS_pidfd_send_signal, fd, sig, info, flags);
}

} // namespace allio::linux

#include <allio/linux/detail/undef.i>
