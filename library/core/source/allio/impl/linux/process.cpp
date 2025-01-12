#include <allio/impl/linux/process.hpp>

#include <allio/impl/linux/poll.hpp>

#include <sys/wait.h>
#include <linux/wait.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

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
		vsm_try_void(linux::poll(fd, POLLIN, deadline));
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
			return vsm::unexpected(static_cast<system_error>(e));
		}

		if (deadline.is_trivial())
		{
			// Polling is required to wait for non-child processes. If the deadline is trivial,
			// the file descriptor was also not polled previously in order to implement a timeout.
			vsm_try_void(linux::poll(fd, POLLIN, deadline));
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

		return vsm::unexpected(error::operation_timed_out);
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
