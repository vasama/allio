#include <allio/impl/linux/process_handle.hpp>

#include <allio/impl/linux/api_string.hpp>
#include <allio/impl/linux/error.hpp>

#include <algorithm>
#include <thread>
#include <vector>

#include <poll.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <spawn.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::linux;

result<unique_fd> linux::open_process(pid_t const pid)
{
	unsigned flags = 0;

	int const fd = syscall(SYS_pidfd_open, pid, flags);
	if (fd == -1)
	{
		return allio_ERROR(get_last_error());
	}
	return result<unique_fd>(result_value, fd);
}


struct raw_process_info
{
	int pid_fd;
	pid_t pid;
};

struct raw_process_info_message
{
	system_error error;
	pid_t pid;
};

static system_result<void> send_result(int const socket, system_result<raw_process_info> const result)
{
	raw_process_info_message message =
	{
		.error = result ? system_error::none : result.error(),
		.pid = result ? result->pid : -1,
	};
	std::byte control_buffer alignas(cmsghdr)[CMSG_SPACE(sizeof(int))];

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

	if (result)
	{
		header.msg_control = control_buffer;
		header.msg_controllen = sizeof(control_buffer);

		cmsghdr* const control_header = CMSG_FIRSTHDR(&header);
		control_header->cmsg_level = SOL_SOCKET;
		control_header->cmsg_type = SCM_RIGHTS;
		control_header->cmsg_len = CMSG_LEN(sizeof(int));
		memcpy(CMSG_DATA(control_header), &result->pid_fd, sizeof(int));
	}

	switch (sendmsg(socket, &header, 0))
	{
	case static_cast<ssize_t>(-1):
		return allio_ERROR(get_last_error());

	default:
		return allio_ERROR(/*TBD*/);

	case static_cast<ssize_t>(sizeof(message)):
		break;
	}

	return {};
}

static system_result<raw_process_info> recv_result(int const socket)
{
	raw_process_info_message message;
	std::byte control_buffer alignas(cmsghdr)[CMSG_SPACE(sizeof(int))];

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

	switch (recvmsg(socket, &header, 0))
	{
	case static_cast<ssize_t>(-1):
		return allio_ERROR(get_last_error());

	default:
		return allio_ERROR(/*TBD*/);

	case static_cast<ssize_t>(sizeof(message)):
		break;
	}

	if (message.error != system_error::none)
	{
		return allio_ERROR(message.error);
	}

	cmsghdr const* const control_header = CMSG_FIRSTHDR(&header);
	if (control_header == nullptr ||
		control_header->cmsg_level != SOL_SOCKET ||
		control_header->cmsg_type != SCM_RIGHTS ||
		control_header->cmsg_len != CMSG_LEN(sizeof(int)))
	{
		return allio_ERROR(/*TBD*/);
	}

	int pid_fd;
	memcpy(&pid_fd, CMSG_DATA(control_header), sizeof(int));

	return raw_process_info
	{
		.pid_fd = pid_fd,
		.pid = message.pid,
	};
}


[[noreturn]] static void exit_process(bool const success)
{
	_exit(success ? EXIT_SUCCESS : EXIT_FAILURE);
}

struct create_process_parameters
{
	int dir_fd;
	char const* path;
	char const* wdir;
	char const* const* argv;
	char const* const* envp;
	int flags;
};

static system_error exec_target_process(create_process_parameters const& args)
{
	if (args.wdir != nullptr && chdir(args.wdir) == -1)
	{
		return get_last_error();
	}

	// When execveat returns, its return value is always -1.
	(void)execveat(args.dir_fd, args.path, args.argv, args.envp, args.flags);

	return get_last_error();
}

template<typename Descriptor>
struct stream_pair
{
	Descriptor read;
	Descriptor write;
};

static system_result<stream_pair<int>> create_pipe_pair()
{
	int fds[2];
	if (pipe2(fds, FD_CLOEXEC) == -1)
	{
		return allio_ERROR(get_last_error());
	}
	return system_result<stream_pair>(result_value, fds[0], fds[1]);
}

static system_result<raw_process_info> create_target_process(create_process_parameters const& args)
{
	allio_TRY(pipe, create_pipe_pair());

	int pid_fd;
	clone_args clone_args =
	{
		.flags = CLONE_PIDFD,
		.pidfd = reinterpret_cast<uintptr_t>(&pid_fd),
	};
	pid_t const target_pid = clone3(&clone_args, sizeof(clone_args));

	if (target_pid == 0)
	{
		system_error const exec_error = exec_target_process(args);
		exit_process(write(pipe.write.get(), &exec_error, sizeof(exec_error)) != sizeof(exec_error));
	}

	if (target_pid == -1)
	{
		return allio_ERROR(get_last_error());
	}

	if (close(pipe.write) == -1)
	{
		return allio_ERROR(get_last_error());
	}

	switch (system_error exec_error; read(pipe.read.get(), &exec_error, sizeof(exec_error)))
	{
	case static_cast<ssize_t>(-1):
		if (system_error const pipe_error = get_last_error(); pipe_error != static_cast<system_error>(EPIPE))
		{
			return allio_ERROR(pipe_error);
		}
		break;

	case static_cast<ssize_t>(sizeof(exec_error)):
		return allio_ERROR(exec_error);

	default:
		return allio_ERROR(/*TBD*/);
	}

	return raw_process_info
	{
		.pid_fd = pid_fd,
		.pid = target_pid,
	};
}

static system_result<stream_pair<unique_fd>> create_socket_pair()
{
	int fds[2];
	if (socketpair(AF_UNIX, SOCK_CLOEXEC, 0, fds) == -1)
	{
		return allio_ERROR(get_last_error());
	}
	return result<socket_pair>(result_value, fds[0], fds[1]);
}

static system_result<raw_process_info> create_orphan_process(create_process_parameters const& args)
{
	allio_TRY(sockets, create_socket_pair());

	clone_args clone_args =
	{
	};
	pid_t const helper_pid = clone3(&clone_args, sizeof(clone_args));

	if (helper_pid == 0)
	{
		exit_process(send_result(sockets.write.get(), create_target_process(args)));
	}

	if (helper_pid == -1)
	{
		return allio_ERROR(get_last_error());
	}

	if (int wait_status; waitpid(helper_pid, &wait_status, 0) == -1)
	{
		return allio_ERROR(get_last_error());
	}
	else if (wait_status != 0)
	{
		if (wait_status == 1)
		{
			return allio_ERROR(/*TBD*/);
		}
		else
		{
			return allio_ERROR(/*TBD*/);
		}
	}

	return recv_result(sockets.read.get());
}


static result<unique_fd> open_current_directory()
{
	int const fd = openat(AT_FDCWD, ".", O_RDONLY | O_DIRECTORY);
	if (fd == -1)
	{
		return allio_ERROR(get_last_error());
	}
	return result<unique_fd>(result_value, fd);
}

result<process_info> linux::launch_process(filesystem_handle const* const base, path_view const path, process_handle::launch_parameters const& args)
{
	allio_TRY(api_path, api_string::make(path));
	allio_TRY(api_argv, api_strings::make(args.arguments));
	allio_TRY(api_envp, api_strings::make(args.environment));

	create_process_parameters create_args =
	{
		.dir_fd = base != nullptr
			? unwrap_handle(base->get_platform_handle())
			: AT_FDCWD,
		.path = api_path,
		.argv = api_argv,
		.envp = api_envp,
	};

	unique_fd current_dir_fd;
	api_string wdir_string;

	if (args.working_directory)
	{
		if (base == nullptr && path.is_relative())
		{
			allio_TRYA(current_dir_fd, open_current_directory());
			create_args.dir_fd = current_dir_fd.get();
		}
		allio_TRYA(create_args.wdir, wdir_string.set(*args.working_directory));
	}

	allio_TRY(process, create_orphan_process(create_args));
	return result<process_info>(result_value, process.pid_fd, static_cast<process_id>(process.pid));

	//auto const r = create_orphan_process(create_args);
	//if (!r)
	//{
	//	return allio_ERROR(std::error_code(r.error(), std::system_category()));
	//}
	//return result<process_info>(result_value, r->pid_fd, static_cast<process_id>(r->pid));
}

#if 0
result<unique_child_pid> linux::launch_process(path_view const path, process_parameters const& args)
{
	pid_t pid;

	if (int const error = posix_spawn(
		&pid,
		api_string(path),
		nullptr,
		nullptr,
		api_strings(args.arguments),
		api_strings(args.environment)))
	{
		return allio_ERROR(std::error_code(error, std::system_category()));
	}

	return result<unique_child_pid>(result_value, pid);
}
#endif


result<void> detail::process_handle_base::do_close_sync()
{
	allio_ASSERT(*this);
	if (static_cast<platform_handle const&>(*this))
	{
		allio_TRYV(platform_handle::do_close_sync());
	}
	if ((m_flags & impl_flags::requires_wait) != process_flags::none)
	{
		allio_TRYV(close_child_pid(m_pid.value));
		m_flags.value &= ~impl_flags::requires_wait;
	}
	m_exit_code.reset();
	m_pid.reset();
	m_flags.reset();
	return {};
}

process_handle detail::process_handle_base::current()
{
	return process_handle(native_handle_type
	{
		{
			{
				process_handle::flags::current
			},
			native_platform_handle::null,
		},
		static_cast<process_id>(getpid()),
	});
}


template<>
struct synchronous_operation_implementation<process_handle, io::process_open>
{
	static result<void> execute(io::parameters_with_result<io::process_open> const& args)
	{
		process_handle& h = *args.handle;

		if (h)
		{
			return allio_ERROR(error::handle_is_not_null);
		}

		allio_TRY(fd, linux::open_process(pid));
		return consume_platform_handle(h, {}, std::move(fd), pid);
	}
};

template<>
struct synchronous_operation_implementation<process_handle, io::process_launch>
{
	static result<void> execute(io::parameters_with_result<io::process_launch> const& args)
	{
		process_handle& h = *args.handle;

		if (!h)
		{
			return allio_ERROR(error::handle_is_null);
		}

		allio_TRY(pid, linux::launch_process(path, args));
		allio_TRY(fd, linux::open_process(pid.get()));
		return detail::consume_resource(std::move(pid), [&](pid_t const pid)
		{
			return consume_platform_handle(h, {}, std::move(fd), pid, impl_flags::requires_wait);
		});
	}
};

template<>
struct synchronous_operation_implementation<process_handle, io::process_wait>
{
	static result<void> execute(io::parameters_with_result<io::process_wait> const& args)
	{
		process_handle& h = *args.handle;

		if (!h)
		{
			return allio_ERROR(error::handle_is_null);
		}

		if (h.is_current())
		{
			return allio_ERROR(error::process_is_current_process);
		}

		if ((h.m_flags.value & process_flags::exited) == process_flags::none)
		{
			int const fd = unwrap_handle(h.get_platform_handle());
			int wait_options = WEXITED | WSTOPPED;

			if (a.deadline != deadline::never())
			{
				if (a.deadline != deadline::instant())
				{
					// Use of poll is required in order to specify a timeout.

					allio_TRYV([&]() -> result<void>
					{
						pollfd pfd =
						{
							.fd = fd,
							.events = EPOLLIN,
						};

						int const r = ppoll(&pfd, 1, kernel_timespec(a.deadline), nullptr);

						if (r == 0)
						{
							return allio_ERROR(error::async_operation_timed_out);
						}

						if (r < 0)
						{
							return allio_ERROR(get_last_error());
						}

						allio_ASSERT(pfd.revents == EPOLLIN);
						return {};
					}());
				}

				wait_options |= WNOHANG;
			}

			allio_TRYS((id_type, id), [&]() -> std::tuple<idtype_t, id_t>
			{
				if ((h.m_flags.value & impl_flags::requires_wait) != process_flags::none)
				{
					return { P_PID, h.m_pid.value };
				}

				return { P_PIDFD, fd };
			}());

			siginfo_t siginfo = {};
			if (waitid(id_type, id, &siginfo, wait_options) == -1)
			{
				return allio_ERROR(get_last_error());
			}

			h.m_flags.value &= ~impl_flags::requires_wait;
			h.m_flags.value |= process_flags::exited;

			h.m_exit_code.value = siginfo.si_code == CLD_EXITED ? siginfo.si_status : 0;
		}

		*args.result = h.m_exit_code.value;
		return {};
	}
};

allio_HANDLE_IMPLEMENTATION(process_handle);
