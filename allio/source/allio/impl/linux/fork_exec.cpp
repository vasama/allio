#include <allio/impl/linux/fork_exec.hpp>

#include <allio/linux/detail/undef.i>

#include <cerrno>
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

class _unique_fd
{
	int m_fd = -1;

public:
	_unique_fd() = default;

	_unique_fd(_unique_fd const&) = delete;
	_unique_fd& operator=(_unique_fd const&) = delete;

	~_unique_fd()
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

	void swap(_unique_fd& other) &
	{
		int const t = m_fd;
		m_fd = other.m_fd;
		other.m_fd = t;
	}
};

struct stream_pair
{
	_unique_fd r;
	_unique_fd w;
};

struct result_storage
{
	_unique_fd pid_fd;
	_unique_fd dup_fd;
	pid_t pid = 0;
};

struct result_message
{
	int error;
	pid_t pid;
};

struct result_pid_fds
{
	int pid_fd;
	int dup_fd;
};

} // namespace


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

static int exec_target(fork_exec_data& data)
{
	if (data.wdir_base != -1 && fchdir(data.wdir_base) == -1)
	{
		return errno;
	}

	if (data.wdir_path != nullptr && chdir(data.wdir_path) == -1)
	{
		return errno;
	}

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

[[noreturn]] void target_entry_point(fork_exec_data& data, int const r_pipe, int const w_pipe)
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


/* In the helper process */

static int fork_target(fork_exec_data& data, result_storage& result)
{
	// Child-to-parent pipe.
	// The child sends an int error code or nothing.
	stream_pair c_p_pipe;

	// Parent-to-child pipe.
	// The parent send a single byte or nothing.
	stream_pair p_c_pipe;

	if (int const e = create_pipe_pair(c_p_pipe))
	{
		return e;
	}

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
		.flags = CLONE_PIDFD,
		.pidfd = reinterpret_cast<uintptr_t>(&pid_fd),
	};
	pid_t const target_pid = clone3(clone_args);

	if (target_pid == 0)
	{
		target_entry_point(data, p_c_pipe.r.get(), c_p_pipe.w.get());
	}

	if (target_pid == -1)
	{
		return errno;
	}

	result.pid_fd.set(pid_fd);
	result.pid = target_pid;

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
			// dup_fd is already inheritable, so swap
			// the two otherwise identical file descriptors.
			result.pid_fd.swap(result.dup_fd);
		}
	}
	else if (data.inheritable_fd)
	{
		int const flags = fcntl(pid_fd, F_GETFD);

		if (flags == -1)
		{
			return errno;
		}

		// Clear the FD_CLOEXEC flag.
		if (fcntl(pid_fd, F_SETFD, 0) == -1)
		{
			return errno;
		}
	}

	if (p_c_pipe.w.get() != -1)
	{
		// Signal the child process to proceed with exec.
		if (write(p_c_pipe.w.get(), "", 1) != 1)
		{
			return errno;
		}
	}

	// Close the write end of the pipe.
	// The child holds it open until exec or exit.
	c_p_pipe.w.set(-1);

	// Wait for the child to exec or exit by reading on the pipe.
	// When the write end is closed, the read completes with 0 bytes read.
	switch (int target_e; read(c_p_pipe.r.get(), &target_e, sizeof(target_e)))
	{
	case 0:
		break;

	case static_cast<ssize_t>(-1):
		return errno;

	case static_cast<ssize_t>(sizeof(target_e)):
		return target_e;

	default:
		return -1; //TODO
	}

	return 0;
}

static int send_result(int const socket, int const error, result_storage& result)
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
		int pid_fd_count = 0;

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

//TODO: Set CLOEXEC on the received FDs.
static int recv_result(int const socket, result_storage& result)
{
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
	stream_pair sockets;
	if (int const e = create_socket_pair(sockets))
	{
		return e;
	}

	clone_args clone_args =
	{
	};
	pid_t const helper_pid = clone3(clone_args);

	if (helper_pid == 0)
	{
		helper_entry_point(data, sockets.w.get());
	}

	if (helper_pid == -1)
	{
		return errno;
	}

	int wait_status;
	if (waitpid(helper_pid, &wait_status, __WCLONE) != helper_pid)
	{
		return errno;
	}

	if (wait_status != EXIT_SUCCESS)
	{
		return -2; //TODO
	}

	if (int const e = recv_result(sockets.r.get(), result))
	{
		return e;
	}

	return 0;
}

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
