#include <allio/impl/linux/process_handle.hpp>

#include <allio/impl/linux/api_string.hpp>
#include <allio/impl/linux/error.hpp>
#include <allio/impl/linux/filesystem_handle.hpp>
#include <allio/linux/timeout.hpp>

#include <vsm/lazy.hpp>
#include <vsm/utility.hpp>

#include <algorithm>
#include <thread>
#include <vector>

#include <linux/sched.h>
#include <poll.h>
#include <sched.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <linux/wait.h> // This header breaks <sys/wait.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

namespace {

static pid_t clone3(clone_args* const cl_args, size_t const size)
{
	return static_cast<pid_t>(syscall(SYS_clone3, cl_args, size));
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
		return vsm::unexpected(get_last_error());

	case static_cast<ssize_t>(sizeof(message)):
		break;

	default:
		vsm_assert(false && "Communication failure on a local socket.");
		return vsm::unexpected(static_cast<system_error>(ENOTRECOVERABLE));
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
		return vsm::unexpected(get_last_error());

	case static_cast<ssize_t>(sizeof(message)):
		break;

	default:
		vsm_assert(false && "Communication failure on a local socket.");
		return vsm::unexpected(static_cast<system_error>(ENOTRECOVERABLE));
	}

	if (message.error != system_error::none)
	{
		return vsm::unexpected(message.error);
	}

	cmsghdr const* const control_header = CMSG_FIRSTHDR(&header);
	if (control_header == nullptr ||
		control_header->cmsg_level != SOL_SOCKET ||
		control_header->cmsg_type != SCM_RIGHTS ||
		control_header->cmsg_len != CMSG_LEN(sizeof(int)))
	{
		vsm_assert(false && "Communication failure on a local socket.");
		return vsm::unexpected(static_cast<system_error>(ENOTRECOVERABLE));
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
	int exec_base;
	int exec_flags;
	char const* exec_path;
	char* const* exec_argv;
	char* const* exec_envp;

	int wdir_base;
	char const* wdir_path;
};

static system_error exec_target_process(create_process_parameters const& args)
{
	if (args.wdir_base != -1 && fchdir(args.wdir_base) == -1)
	{
		return get_last_error();
	}

	if (args.wdir_path != nullptr && chdir(args.wdir_path) == -1)
	{
		return get_last_error();
	}

	// When execveat returns, its return value is always -1.
	(void)execveat(args.exec_base, args.exec_path, args.exec_argv, args.exec_envp, args.exec_flags);

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
		return vsm::unexpected(get_last_error());
	}
	return system_result<stream_pair<int>>(vsm::result_value, fds[0], fds[1]);
}

static system_result<raw_process_info> create_target_process(create_process_parameters const& args)
{
	vsm_try(pipe, create_pipe_pair());

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
		exit_process(write(pipe.write, &exec_error, sizeof(exec_error)) != sizeof(exec_error));
	}

	if (target_pid == -1)
	{
		return vsm::unexpected(get_last_error());
	}

	if (close(pipe.write) == -1)
	{
		return vsm::unexpected(get_last_error());
	}

	switch (system_error exec_error; read(pipe.read, &exec_error, sizeof(exec_error)))
	{
	case static_cast<ssize_t>(-1):
		if (system_error const pipe_error = get_last_error(); pipe_error != static_cast<system_error>(EPIPE))
		{
			return vsm::unexpected(pipe_error);
		}
		break;

	case static_cast<ssize_t>(sizeof(exec_error)):
		return vsm::unexpected(exec_error);

	default:
		vsm_assert(false && "Communication failure on a local socket.");
		return vsm::unexpected(static_cast<system_error>(ENOTRECOVERABLE));
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
		return vsm::unexpected(get_last_error());
	}
	return system_result<stream_pair<unique_fd>>(vsm::result_value, unique_fd(fds[0]), unique_fd(fds[1]));
}

static system_result<raw_process_info> create_orphan_process(create_process_parameters const& args)
{
	vsm_try(sockets, create_socket_pair());

	clone_args clone_args =
	{
	};
	pid_t const helper_pid = clone3(&clone_args, sizeof(clone_args));

	if (helper_pid == 0)
	{
		exit_process(send_result(sockets.write.get(), create_target_process(args)).has_value());
	}

	if (helper_pid == -1)
	{
		return vsm::unexpected(get_last_error());
	}

	if (int wait_status; waitpid(helper_pid, &wait_status, 0) == -1)
	{
		return vsm::unexpected(get_last_error());
	}
	else if (wait_status != EXIT_SUCCESS)
	{
		vsm_assert(false && "Communication failure on a local socket.");
		return vsm::unexpected(static_cast<system_error>(ENOTRECOVERABLE));
	}

	return recv_result(sockets.read.get());
}

} // namespace


vsm::result<process_id> _process_handle::get_process_id() const
{
	//TODO: Parse /proc/self/fdinfo/{fd}
	//      See https://stackoverflow.com/questions/74555660/given-a-pid-fd-as-acquired-from-pidfd-open-how-does-one-get-the-underlying
	return vsm::unexpected(error::unsupported_operation);
}


vsm::result<unique_fd> linux::open_process(pid_t const pid)
{
	unsigned flags = 0;

	int const fd = syscall(SYS_pidfd_open, pid, flags);
	if (fd == -1)
	{
		return vsm::unexpected(get_last_error());
	}
	return vsm::result<unique_fd>(vsm::result_value, fd);
}

vsm::result<process_info> linux::launch_process(
	filesystem_handle const* const base,
	input_path_view const path,
	process_handle::launch_parameters const& args)
{
	create_process_parameters create_args =
	{
		.exec_base = base != nullptr
			? unwrap_handle(base->get_platform_handle())
			: AT_FDCWD,

		.wdir_base = -1,
	};

	unique_fd exec_fd;
	api_string_storage wdir_path_storage;

	if (args.working_directory)
	{
		input_path const& wdir = *args.working_directory;

		if (wdir.base != nullptr || !wdir.path.empty())
		{
			// If the executable path is relative its meaning
			// would change with change of working directory.
			if (base == nullptr && path.utf8_path().is_relative())
			{
				open_parameters const open_args =
				{
					.flags = O_DIRECTORY | O_CLOEXEC,
					.mode = O_RDONLY,
				};

				// Open the executable file to deal with the change of working directory.
				vsm_try_assign(exec_fd, create_file(base, path, open_args));

				create_args.exec_base = exec_fd.get();
				create_args.exec_flags |= AT_EMPTY_PATH;
			}

			if (wdir.base != nullptr && wdir.path.utf8_path().is_relative())
			{
				create_args.wdir_base = unwrap_handle(wdir.base->get_platform_handle());
			}
		}
	}

	api_string_storage string_storage;
	vsm_try_void(api_string_builder::make(string_storage, [&](auto&& context)
	{
		if (!exec_fd)
		{
			create_args.exec_path = context(path).data();
		}

		if (!args.arguments.empty())
		{
			create_args.exec_argv = const_cast<char* const*>(context(args.arguments));
		}

		if (!args.environment.empty())
		{
			create_args.exec_envp = const_cast<char* const*>(context(args.environment));
		}

		if (args.working_directory)
		{
			create_args.wdir_path = context(args.working_directory->path).data();
		}
	}));

	vsm_try(process, create_orphan_process(create_args));

	return vsm_lazy(process_info
	{
		.pid_fd = unique_fd(process.pid_fd),
		.pid = static_cast<process_id>(process.pid),
	});
}


#if 0
static vsm::result<unique_fd> open_current_directory()
{
	int const fd = openat(AT_FDCWD, ".", O_RDONLY | O_DIRECTORY);
	if (fd == -1)
	{
		return vsm::unexpected(get_last_error());
	}
	return vsm::result<unique_fd>(vsm::result_value, fd);
}
#endif


vsm::result<void> process_handle_base::sync_impl(io::parameters_with_result<io::close> const& args)
{
	platform_handle& h = *args.handle;
	vsm_assert(h);
	
	vsm_try_void(base_type::sync_impl(args));

	h.m_pid.reset();
	h.m_exit_code.reset();
	
	return {};
}

process_handle process_handle_base::current()
{
	static native_handle_type const native =
	{
		platform_handle::native_handle_type
		{
			handle::native_handle_type
			{
				handle_flags(flags::not_null) | flags::current,
			},
			native_platform_handle::null,
		},
		static_cast<process_id>(getpid()),
	};

	process_handle h;
	vsm_verify(h.set_native_handle(native));
	return h;
}


vsm::result<void> process_handle_base::sync_impl(io::parameters_with_result<io::process_open> const& args)
{
	process_handle& h = *args.handle;

	if (h)
	{
		return vsm::unexpected(error::handle_is_not_null);
	}

	vsm_try(fd, linux::open_process(args.pid));

	return initialize_platform_handle(h, vsm_move(fd),
		[&](native_platform_handle const handle)
		{
			return process_handle::native_handle_type
			{
				platform_handle::native_handle_type
				{
					handle::native_handle_type
					{
						flags::not_null,
					},
					handle,
				},
				args.pid,
			};
		}
	);
}

vsm::result<void> process_handle_base::sync_impl(io::parameters_with_result<io::process_launch> const& args)
{
	process_handle& h = *args.handle;

	if (!h)
	{
		return vsm::unexpected(error::handle_is_null);
	}

	vsm_try_bind((fd, pid), linux::launch_process(args.base, args.path, args));

	return initialize_platform_handle(h, vsm_move(fd),
		[&](native_platform_handle const handle)
		{
			return process_handle::native_handle_type
			{
				platform_handle::native_handle_type
				{
					handle::native_handle_type
					{
						flags::not_null,
					},
					handle,
				},
				pid,
			};
		}
	);
}

vsm::result<void> process_handle_base::sync_impl(io::parameters_with_result<io::process_wait> const& args)
{
	process_handle& h = *args.handle;

	if (!h)
	{
		return vsm::unexpected(error::handle_is_null);
	}

	if (h.is_current())
	{
		return vsm::unexpected(error::process_is_current_process);
	}

	if (!h.get_flags()[flags::exited])
	{
		int const fd = unwrap_handle(h.get_platform_handle());
		int wait_options = WEXITED | WSTOPPED;

		if (args.deadline != deadline::never())
		{
			if (args.deadline != deadline::instant())
			{
				// Use of poll is required in order to specify a timeout.

				vsm_try_void([&]() -> vsm::result<void>
				{
					pollfd pfd =
					{
						.fd = fd,
						.events = POLLIN,
					};

					int const r = ppoll(&pfd, 1, kernel_timeout<timespec>(args.deadline), nullptr);

					if (r == 0)
					{
						return vsm::unexpected(error::async_operation_timed_out);
					}

					if (r < 0)
					{
						return vsm::unexpected(get_last_error());
					}

					vsm_assert(pfd.revents == POLLIN);

					return {};
				}());
			}

			wait_options |= WNOHANG;
		}

		static constexpr auto id_pidfd = static_cast<idtype_t>(P_PIDFD);

		siginfo_t siginfo = {};
		if (waitid(id_pidfd, h.m_pid.value, &siginfo, wait_options) == -1)
		{
			return vsm::unexpected(get_last_error());
		}

		h.set_flags(h.get_flags() | flags::exited);

		//TODO: Exit code for stopped processes?
		h.m_exit_code.value = siginfo.si_code == CLD_EXITED ? siginfo.si_status : 0;
	}

	*args.result = h.m_exit_code.value;

	return {};
}

allio_handle_implementation(process_handle);
