#include <allio/impl/linux/process.hpp>

#include <allio/impl/linux/poll.hpp>

#include <sys/ioctl.h>
#include <sys/wait.h>
#include <linux/types.h>
#include <linux/wait.h>

#include <allio/linux/detail/undef.i>

namespace allio::linux::pidfd {
#ifndef PIDFD_GET_INFO

struct pidfd_info
{
	__u64 mask;
	__u64 cgroupid;
	__u32 pid;
	__u32 tgid;
	__u32 ppid;
	__u32 ruid;
	__u32 rgid;
	__u32 euid;
	__u32 egid;
	__u32 suid;
	__u32 sgid;
	__u32 fsuid;
	__u32 fsgid;
	__s32 exit_code;
	__u32 coredump_mask;
	__u32 __spare1;
};

#define PIDFD_INFO_PID          (1UL << 0)
#define PIDFD_INFO_EXIT         (1UL << 3)

#define PIDFS_IOCTL_MAGIC       0xFF
#define PIDFD_GET_INFO          _IOWR(PIDFS_IOCTL_MAGIC, 11, pidfd_info)

#endif // PIDFD_GET_INFO
} // namespace allio::linux::pidfd
using namespace allio::linux::pidfd;

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

static vsm::result<void> get_pidfd_info(
	int const fd,
	uint64_t const mask,
	pidfd_info& out_info)
{
	out_info.mask = mask;
	int const r = ::ioctl(fd, PIDFD_GET_INFO, &out_info);

	if (r < 0)
	{
		return vsm::unexpected(allio_error(get_last_error()));
	}

	if ((out_info.mask & mask) != mask)
	{
		return vsm::unexpected(allio_error(error::unknown_failure));
	}

	return {};
}

vsm::result<int> linux::get_pid(int const fd)
{
	pidfd_info info;
	vsm_try_void(get_pidfd_info(fd, PIDFD_INFO_PID, info));
	return info.pid;
}

vsm::result<int> linux::get_exit_code(int const fd)
{
	pidfd_info info;
	vsm_try_void(get_pidfd_info(fd, PIDFD_INFO_EXIT, info));
	return info.exit_code;
}

vsm::result<std::optional<int>> linux::wait_process(
	int const fd,
	bool const reap,
	deadline const deadline)
{
	int flags = WEXITED;

	if (!reap)
	{
		flags |= WNOWAIT;
	}

	if (!deadline.is_trivial())
	{
		// Polling is required to specify a non-trivial timeout.
		vsm_try_discard(linux::poll(fd, POLLIN, deadline));
	}
	else if (deadline == deadline::instant())
	{
		flags |= WNOHANG;
	}

	siginfo_t siginfo;
	siginfo.si_pid = 0;

	int const r = waitid(
		static_cast<idtype_t>(P_PIDFD),
		static_cast<id_t>(fd),
		&siginfo,
		flags);

	if (r == -1)
	{
		if (int const e = errno; e != ECHILD)
		{
			return vsm::unexpected(allio_error(static_cast<system_error>(e)));
		}

		if (deadline.is_trivial())
		{
			// Polling is required to wait for non-child processes. If the deadline is trivial,
			// the file descriptor was also not polled previously in order to implement a timeout.
			vsm_try_discard(linux::poll(fd, POLLIN, deadline));
		}

		// In any case, the exit code of a non-child process cannot be made available.
		return std::nullopt;
	}

	if (siginfo.si_pid == 0)
	{
		// If WNOHANG was specified, a si_pid retaining the value of zero indicates that the process
		// has not yet terminated. In any other case, si_pid should have been overwritten with the
		// pid of the terminated process.
		vsm_assert(flags & WNOHANG);

		return vsm::unexpected(allio_error(error::operation_timed_out));
	}

	//TODO: Should the exit code be different when the process was killed?
	//      See what other libraries are doing here.

	// If si_status describes a signal, add 128 to match common shell behaviour. This is used to
	// distinguish between user-specified exit codes in the range [0, 127] and signals. Otherwise
	// the user-specified exit code is truncated to its low 8 bits. This is most likely redundant,
	// as the kernel has already truncated it, but it is also done here as a defensive measure to
	// proof against an allio API break due to future kernels returning the full exit code.
	return siginfo.si_code == CLD_EXITED
		? siginfo.si_status & 0xFF
		: siginfo.si_status + 128;
}
