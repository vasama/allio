#include <allio/handles/process.hpp>

#include <allio/impl/linux/api_string.hpp>
#include <allio/impl/linux/error.hpp>
#include <allio/impl/linux/fcntl.hpp>
#include <allio/impl/linux/fork_exec.hpp>
#include <allio/impl/linux/handles/fs_object.hpp>
#include <allio/impl/linux/pidfd.hpp>
#include <allio/impl/linux/poll.hpp>
#include <allio/impl/linux/proc.hpp>
#include <allio/impl/linux/process_reaper.hpp>

#include <vsm/numeric.hpp>
#include <vsm/utility.hpp>

#include <algorithm>
#include <thread>
#include <vector>

#include <sys/wait.h>

#include <linux/wait.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

#if 0
static vsm::result<pid_t> get_process_id(int const fd)
{
	vsm_try(file, proc_open("/proc/self/fdinfo/%d", fd));

	long long pid;
	vsm_try_void(proc_scan(file,
		"pos: %*lli\n"
		"flags: %*o\n"
		"mnt_id: %*i\n"
		"ino: %*lu\n"
		"Pid: %lli\n", &pid));

	if (pid == 0)
	{
		return vsm::unexpected(error::process_id_not_available);
	}

	// Linux PIDs are in the range [1, 2^22] and further limited
	// to at most 2^31-1 by the 32-bit integers used to store them.
	// Despite the kernel using long long for printing, there is no
	// reasonable scenario where pid would exceed the range of pid_t.
	vsm_assert(0 < pid && pid < std::numeric_limits<pid_t>::max());

	return static_cast<pid_t>(pid);
}
#endif


vsm::result<void> process_t::open(
	native_type& h,
	io_parameters_t<process_t, open_t> const& a)
{
	vsm_try(pid, vsm::try_truncate<pid_t>(
		a.id.integer(),
		error::invalid_argument));

	vsm_try(fd, linux::pidfd_open(
		pid,
		/* flags: */ 0));

	if (a.inheritable)
	{
		// Linux currently only supports the FD_CLOEXEC flag,
		// and it is implicitly assigned by pidfd_open.
		vsm_assert(linux::fcntl(fd.get(), F_GETFD) == FD_CLOEXEC);

		// Remove the FD_CLOEXEC flag.
		vsm_try_discard(linux::fcntl(fd.get(), F_SETFD, /* flags: */ 0));
	}

	h = native_type
	{
		platform_object_t::native_type
		{
			object_t::native_type
			{
				flags::not_null,
			},
			wrap_handle(fd.release()),
		},
		process_id(0),
		a.id,
	};

	return {};
}

vsm::result<void> process_t::launch(
	native_type& h,
	io_parameters_t<process_t, launch_t> const& a)
{
	process_reaper_ptr reaper;
	handle_flags h_flags = {};

	unique_handle pid_fd;
	pid_t pid = 0;

	// Launch the process using the internal fork_exec interface.
	{
		fork_exec_data data =
		{
			.exec_base = a.path.base == native_platform_handle::null
				? AT_FDCWD
				: unwrap_handle(a.path.base),

			.inheritable_fd = a.inheritable,
		};

		api_string_storage string_storage;
		std::string_view exec_path;
		std::string_view wdir_path;
		unique_handle exec_fd;

		vsm_try_void(api_string_builder::make(string_storage, [&](auto&& ctx)
		{
			exec_path = ctx.string(a.path.path.string());

			if (!a.command_line.empty())
			{
				data.exec_argv = const_cast<char* const*>(ctx.strings(a.path.path.string(), a.command_line));
			}

			if (a.environment)
			{
				data.exec_envp = const_cast<char* const*>(ctx.strings(*a.environment));
			}

			if (a.working_directory)
			{
				wdir_path = ctx.string(a.working_directory->path.string());
			}
		}));
		data.exec_path = exec_path.data();

		if (a.working_directory)
		{
			data.wdir_path = wdir_path.data();

			// If the executable path is relative, its meaning
			// would change with the change of working directory.
			if (a.path.base == native_platform_handle::null &&
				path_view(exec_path).is_relative())
			{
				// Open the executable file to deal with the change of working directory.
				vsm_try_assign(exec_fd, linux::open_file(
					-1,
					exec_path.data(),
					O_PATH | O_CLOEXEC));

				data.exec_base = exec_fd.get();
				data.exec_path = "";
				data.exec_flags |= AT_EMPTY_PATH;
			}

			auto const wdir_base = a.working_directory->base;

			// If a working directory base handle is specified,
			// make use of it only if the path is relative.
			if (wdir_base != native_platform_handle::null &&
				path_view(wdir_path).is_relative())
			{
				data.wdir_base = unwrap_handle(wdir_base);
			}
		}

		if (a.std_input != native_platform_handle::null)
		{
			data.exec_stdin = unwrap_handle(a.std_input);
		}
		if (a.std_output != native_platform_handle::null)
		{
			data.exec_stdout = unwrap_handle(a.std_output);
		}
		if (a.std_error != native_platform_handle::null)
		{
			data.exec_stderr = unwrap_handle(a.std_error);
		}

		if (!a.launch_detached && !a.wait_on_close)
		{
			// A duplicate fd is required for the reaper.
			data.duplicate_fd = true;

			// Create the reaper before launching so that we are not in trouble
			// if its creation fails after the child process was already launched.
			vsm_try_assign(reaper, acquire_process_reaper());
		}

		if (int const error = fork_exec(data))
		{
			return vsm::unexpected(static_cast<system_error>(error));
		}

		pid_fd.reset(data.pid_fd);
		pid = data.pid;

		if (reaper != nullptr)
		{
			start_process_reaper(reaper.get(), data.dup_fd);
		}
		else
		{
			vsm_assert(data.dup_fd == -1);
		}
	}

	if (a.wait_on_close)
	{
		h_flags |= flags::wait_on_close;
	}

	h = native_type
	{
		platform_object_t::native_type
		{
			object_t::native_type
			{
				flags::not_null | h_flags,
			},
			wrap_handle(pid_fd.release()),
		},
		process_id(0),
		process_id(pid),
		reaper.release(),
	};

	return {};
}

vsm::result<void> process_t::terminate(
	native_type const& h,
	io_parameters_t<process_t, terminate_t> const& a)
{
	if (a.exit_code)
	{
		//TODO: Deal with the requested exit code somehow?
		return vsm::unexpected(error::unsupported_operation);
	}

	if (pidfd_send_signal(
		unwrap_handle(h.platform_handle),
		SIGKILL,
		/* siginfo: */ nullptr,
		/* flags: */ 0) == -1)
	{
		// The process might have exited and been reaped already.
		// This is not considered an error.
		if (int const e = errno; e != ESRCH)
		{
			return vsm::unexpected(static_cast<system_error>(e));
		}
	}

	return {};
}

vsm::result<process_exit_code> process_t::wait(
	native_type const& h,
	io_parameters_t<process_t, wait_t> const& a)
{
	if (static_cast<pid_t>(h.id.integer()) == getpid())
	{
		return vsm::unexpected(error::process_is_current_process);
	}

	int const fd = unwrap_handle(h.platform_handle);

	if (h.reaper == nullptr || !a.deadline.is_trivial())
	{
		// Poll is required to wait for non-child processes,
		// or to specify a timeout regardless of the relationship.
		vsm_try_void(linux::poll(fd, POLLIN, a.deadline));
	}

	if (h.reaper == nullptr)
	{
		// A reaper is required in order to wait for the exit code.
		return vsm::unexpected(error::process_exit_code_not_available);
	}

	//TODO: Close the reaper duplicate fd as early as possible.
	return process_reaper_wait(h.reaper, fd);
}

vsm::result<void> process_t::close(
	native_type& h,
	io_parameters_t<process_t, close_t> const& a)
{
	if (h.reaper != nullptr)
	{
		release_process_reaper(h.reaper);
		h.reaper = nullptr;
	}
	return base_type::close(h, a);
}


blocking::process_handle const& this_process::get_handle()
{
	static constexpr auto make_handle = []()
	{
		return blocking::process_handle(
			adopt_handle,
			process_t::native_type
			{
				platform_object_t::native_type
				{
					object_t::native_type
					{
						object_t::flags::not_null,
					},
					native_platform_handle::null,
				},
				process_id{},
				process_id(getpid()),
			}
		);
	};

	static auto const handle = make_handle();
	return handle;
}

vsm::result<blocking::process_handle> this_process::open()
{
	return blocking::open_process(process_id(getpid()));
}
