// This translation unit is intentionally compiled with the smallest possible set of external
// dependencies in order to avoid accidentally executing any external code during the fork
// procedure, as doing so in the forked processes would be unsafe.

#include <allio/impl/linux/fork_exec.hpp>

#include <allio/linux/detail/undef.i>

#include <cerrno>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <fcntl.h>
#include <sched.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#include <linux/sched.h>

using allio::linux::fork_exec_data;

namespace {

class unique_fd
{
	int m_fd = -1;

public:
	unique_fd() = default;

	unique_fd(unique_fd const&) = delete;
	unique_fd& operator=(unique_fd const&) = delete;

	~unique_fd()
	{
		if (m_fd != -1)
		{
			close(m_fd);
		}
	}

	[[nodiscard]] int get() const
	{
		return m_fd;
	}

	void set(int const fd) &
	{
		if (fd != m_fd)
		{
			if (m_fd != -1)
			{
				close(m_fd);
			}
			m_fd = fd;
		}
	}

	[[nodiscard]] int release()
	{
		int const fd = m_fd;
		m_fd = -1;
		return fd;
	}

	void swap(unique_fd& other) &
	{
		int const t = m_fd;
		m_fd = other.m_fd;
		other.m_fd = t;
	}
};

struct stream_pair
{
	unique_fd r;
	unique_fd w;
};

struct result_storage
{
	unique_fd pid_fd;
	unique_fd dup_fd;
	pid_t pid = 0;
};

struct result_message
{
	int error;
	pid_t pid;
};


static pid_t clone3(clone_args& args)
{
	return static_cast<pid_t>(syscall(SYS_clone3, &args, sizeof(args)));
}

[[noreturn]] static void exit_process(bool const success)
{
	_exit(success ? EXIT_SUCCESS : EXIT_FAILURE);
}

static int create_pipe_pair(stream_pair& pipes)
{
	int fd[2];
	if (pipe2(fd, O_CLOEXEC) == -1)
	{
		return errno;
	}
	pipes.r.set(fd[0]);
	pipes.w.set(fd[1]);
	return 0;
}

static int create_socket_pair(stream_pair& sockets)
{
	int fd[2];
	if (socketpair(
		AF_UNIX,
		SOCK_STREAM | SOCK_CLOEXEC,
		/* protocol: */ 0,
		fd) == -1)
	{
		return errno;
	}
	sockets.r.set(fd[0]);
	sockets.w.set(fd[1]);
	return 0;
}


/* In the target process */

// Sets CLOEXEC on all open file descriptors not found in the ordered array.
static int close_other(int const* const array, size_t const count)
{
	int lower_bound = -1;

	for (size_t i = 0; i < count; ++i)
	{
		int const upper_bound = array[i];

		if (lower_bound + 1 <= upper_bound - 1)
		{
			if (close_range(
				static_cast<unsigned>(lower_bound + 1),
				static_cast<unsigned>(upper_bound - 1),
				CLOSE_RANGE_CLOEXEC) == -1)
			{
				return errno;
			}
		}

		lower_bound = upper_bound;
	}

	if (lower_bound != INT_MAX)
	{
		if (close_range(
			static_cast<unsigned>(lower_bound + 1),
			static_cast<unsigned>(INT_MAX),
			CLOSE_RANGE_CLOEXEC) == -1)
		{
			return errno;
		}
	}

	return 0;
}

static int exec_target(fork_exec_data const& data)
{
	// If an inherit fd array is specified, set the CLOEXEC flag on all file descriptors not present
	// in the array. Specifying a non-null but empty array causes all descriptors to be affected.
	if (data.inherit_fd_array != nullptr)
	{
		if (int const e = close_other(data.inherit_fd_array, data.inherit_fd_count))
		{
			return e;
		}
	}

	// If the working directory path base descriptor is set, change the current working directory to
	// the directory referred to by the base descriptor. If the working directory path is absolute,
	// this is pointless but also harmless. The user should ensure that a base descriptor is only
	// set in combination with a relative working directory path.
	if (data.wdir_base != -1 && fchdir(data.wdir_base) == -1)
	{
		return errno;
	}

	// Change the current working directory to the directory referred to by the specified path.
	if (data.wdir_path != nullptr && chdir(data.wdir_path) == -1)
	{
		return errno;
	}

	// Duplicate the standard stream handles into the appropriate positions. dup2 does not set the
	// CLOEXEC flag, which is exactly what is needed for preserving the descriptors across exec. If
	// the descriptor is already in the correct position, this has the effect of clearing CLOEXEC.

	if (data.exec_stdin != -1)
	{
		if (dup2(data.exec_stdin, STDIN_FILENO) == -1)
		{
			return errno;
		}
	}

	if (data.exec_stdout != -1)
	{
		if (dup2(data.exec_stdout, STDOUT_FILENO) == -1)
		{
			return errno;
		}
	}

	if (data.exec_stderr != -1)
	{
		if (dup2(data.exec_stderr, STDERR_FILENO) == -1)
		{
			return errno;
		}
	}

	// When execveat returns, its return value is always -1.
	(void)execveat(
		data.exec_base,
		data.exec_path,
		data.exec_argv,
		data.exec_envp,
		data.exec_flags);

	return errno;
}

// Attempts to exec the target executable. If the exec fails, writes the error code to the output
// pipe and exits. If r_pipe is not -1, waits for the parent process to finish its pre-exec
// initialization by reading from r_pipe, before calling exec. The exec error code is communicated
// to the parent process via w_pipe. The process exit code communicates the success or failure of
// the write on w_pipe.
[[noreturn]] static void target_entry_point(
	fork_exec_data const& data,
	int const r_pipe,
	int const w_pipe)
{
	bool do_exec = true;

	if (r_pipe != -1)
	{
		unsigned char buffer[1];
		do_exec = read(r_pipe, buffer, 1) == 1;
	}

	int const exec_e = do_exec ? exec_target(data) : 0;
	ssize_t const write_size = write(w_pipe, &exec_e, sizeof(exec_e));
	exit_process(write_size == sizeof(exec_e));
}


/* In either the source or the helper process */

static int fork_target(fork_exec_data const& data, result_storage& result)
{
	// Child-to-parent pipe. The child sends an int error code or nothing.
	stream_pair c_p_pipe;

	// Parent-to-child pipe. The parent sends a single byte or nothing.
	stream_pair p_c_pipe;

	if (int const e = create_pipe_pair(c_p_pipe))
	{
		return e;
	}

	// If duplication or inheritability of the pid fd is requested, the target process is instructed
	// to wait for confirmation via the parent-to-child pipe before proceeding with exec.
	if (data.duplicate_fd || data.inheritable_fd)
	{
		if (int const e = create_pipe_pair(p_c_pipe))
		{
			return e;
		}
	}

	int pid_fd;
	clone_args clone_args =
	{
		.flags = CLONE_CLEAR_SIGHAND | CLONE_PIDFD,
		.pidfd = reinterpret_cast<uintptr_t>(&pid_fd),
	};
	pid_t const target_pid = clone3(clone_args);

	if (target_pid == 0)
	{
		// This function - executed in the child process - does not return:
		target_entry_point(data, p_c_pipe.r.get(), c_p_pipe.w.get());
	}

	if (target_pid == -1)
	{
		return errno;
	}

	result.pid_fd.set(pid_fd);
	result.pid = target_pid;

	// If duplication or inheritability of the pid fd is requested, the necessary operations are
	// performed before signaling the child process to proceed with exec. This is done to avoid a
	// situation where a file control operation fails after the child has already called exec.
	if (data.duplicate_fd)
	{
		int const dup_fd = fcntl(
			pid_fd,
			data.inheritable_fd ? F_DUPFD : F_DUPFD_CLOEXEC,
			/* lowest_new_fd: */ 0);

		if (dup_fd == -1)
		{
			return errno;
		}

		result.dup_fd.set(dup_fd);

		if (data.inheritable_fd)
		{
			// dup_fd is already inheritable (due to the flags used), so the two otherwise identical
			// file descriptors are swapped, leaving pid_fd inheritable and dup_fd with CLOEXEC set.
			result.pid_fd.swap(result.dup_fd);
		}
	}
	else if (data.inheritable_fd)
	{
		// Clear the FD_CLOEXEC flag:
		fcntl(pid_fd, F_SETFD, 0);
	}

	if (p_c_pipe.w.get() != -1)
	{
		// Signal the child process to proceed with exec. The value of the byte sent via the pipe
		// has no effect. The child process simply waits for the read to complete successfully.
		if (write(p_c_pipe.w.get(), "", 1) != 1)
		{
			return errno;
		}
	}

	// Close the write end of the child-to-parent pipe. The child process holds it open until
	// calling exec or exit, at which point the following read in the parent process completes.
	c_p_pipe.w.set(-1);

	// Wait for the child to exec or exit by reading from the child-to-parent pipe. When the write
	// end is closed, the read completes with 0 bytes read. This indicates that either exec was
	// called successfully, or that the child process was otherwise unexpectedly terminated. There
	// is no need to distinguish between these two cases, because that unexpected termination could
	// apply to the process after exec equally well.
	switch (int target_e; read(c_p_pipe.r.get(), &target_e, sizeof(target_e)))
	{
	case 0:
		return 0;

	// Failure in reading is unexpected:
	case static_cast<ssize_t>(-1):
		return errno;

	case static_cast<ssize_t>(sizeof(target_e)):
		return target_e;

	// Any size other than the size of the error code is unexpected:
	default:
		return -1; //TODO
	}
}


/* In the helper process */

// Send the error code along with the target process pid fds (one or two depending on duplication)
// and target pid to the parent process using the shared unix domain socket.
static int send_result(int const socket, int const error, result_storage const& result)
{
	result_message message =
	{
		.error = error,
		.pid = result.pid,
	};
	unsigned char control_buffer alignas(cmsghdr)[CMSG_SPACE(2 * sizeof(int))];

	iovec io_vector =
	{
		.iov_base = &message,
		.iov_len = sizeof(message),
	};

	msghdr header =
	{
		.msg_iov = &io_vector,
		.msg_iovlen = 1,
	};

	if (error == 0)
	{
		int pid_fd_array[2];
		size_t pid_fd_count = 0;

		pid_fd_array[pid_fd_count++] = result.pid_fd.get();
		if (result.dup_fd.get() != -1)
		{
			pid_fd_array[pid_fd_count++] = result.dup_fd.get();
		}

		size_t const data_size = pid_fd_count * sizeof(int);

		header.msg_control = control_buffer;
		header.msg_controllen = sizeof(control_buffer);

		cmsghdr* const control_header = CMSG_FIRSTHDR(&header);
		control_header->cmsg_level = SOL_SOCKET;
		control_header->cmsg_type = SCM_RIGHTS;
		control_header->cmsg_len = CMSG_LEN(data_size);

		memcpy(CMSG_DATA(control_header), pid_fd_array, data_size);
	}

	switch (sendmsg(
		socket,
		&header,
		MSG_NOSIGNAL))
	{
	case static_cast<ssize_t>(-1):
		return errno;

	case static_cast<ssize_t>(sizeof(message)):
		break;

	default:
		return -3; //TODO
	}

	return 0;
}

[[noreturn]] static void helper_entry_point(fork_exec_data& data, int const socket)
{
	result_storage result;
	int const fork_e = fork_target(data, result);
	int const send_e = send_result(socket, fork_e, result);
	exit_process(send_e == 0);
}


/* In the source process */

// Receive the error code along with the target process pid fds (one or two depending on
// duplication) and target pid from the helper process using the shared unix domain socket.
static int recv_result(int const socket, result_storage& result)
{
	//TODO: Set CLOEXEC on the received FDs.

	result_message message;
	unsigned char control_buffer alignas(cmsghdr)[CMSG_SPACE(2 * sizeof(int))];

	iovec io_vector =
	{
		.iov_base = &message,
		.iov_len = sizeof(message),
	};

	msghdr header =
	{
		.msg_iov = &io_vector,
		.msg_iovlen = 1,
		.msg_control = control_buffer,
		.msg_controllen = sizeof(control_buffer),
	};

	switch (recvmsg(
		socket,
		&header,
		MSG_CMSG_CLOEXEC))
	{
	case static_cast<ssize_t>(-1):
		return errno;

	case static_cast<ssize_t>(sizeof(message)):
		break;

	default:
		return -4; //TODO
	}

	if (message.error != 0)
	{
		return message.error;
	}

	cmsghdr const* const control_header = CMSG_FIRSTHDR(&header);

	if (control_header == nullptr ||
		control_header->cmsg_level != SOL_SOCKET ||
		control_header->cmsg_type != SCM_RIGHTS)
	{
		return -5; //TODO
	}

	size_t data_size;
	switch (control_header->cmsg_len)
	{
	case CMSG_LEN(1 * sizeof(int)):
		data_size = 1 * sizeof(int);
		break;

	case CMSG_LEN(2 * sizeof(int)):
		data_size = 2 * sizeof(int);
		break;

	default:
		return -5; //TODO
	}

	int pid_fd_array[2] = { -1, -1 };
	memcpy(pid_fd_array, CMSG_DATA(control_header), data_size);

	result.pid_fd.set(pid_fd_array[0]);
	result.dup_fd.set(pid_fd_array[1]);
	result.pid = message.pid;

	return 0;
}

static int fork_helper(fork_exec_data& data, result_storage& result)
{
	stream_pair socket_pair;
	if (int const e = create_socket_pair(socket_pair))
	{
		return e;
	}

	clone_args clone_args =
	{
		.flags = CLONE_CLEAR_SIGHAND,
	};
	pid_t const helper_pid = clone3(clone_args);

	if (helper_pid == 0)
	{
		// This function - executed in the helper process - does not return:
		helper_entry_point(data, socket_pair.w.get());
	}

	if (helper_pid == -1)
	{
		return errno;
	}

	int wait_status;
	if (waitpid(helper_pid, &wait_status, static_cast<int>(__WCLONE)) != helper_pid)
	{
		return errno;
	}

	if (wait_status != EXIT_SUCCESS)
	{
		return -2; //TODO
	}

	if (int const e = recv_result(socket_pair.r.get(), result))
	{
		return e;
	}

	// TODO: pid_fd always has CLOEXEC (due to MSG_CMSG_CLOEXEC). The helper and target process
	//       launch should be restructured in such a way that it is always the root process which
	//       signals the target process to proceed with exec. The parent-to-child pipe should be
	//       created by the root process.

	return 0;
}

} // namespace


int allio::linux::fork_exec(fork_exec_data& data)
{
	result_storage result;

	if (data.fork_detached)
	{
		if (int const e = fork_helper(data, result))
		{
			return e;
		}
	}
	else
	{
		if (int const e = fork_target(data, result))
		{
			return e;
		}
	}

	data.pid_fd = result.pid_fd.release();
	data.dup_fd = result.dup_fd.release();
	data.pid = result.pid;

	return 0;
}
