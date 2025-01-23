#include <allio/handles/process.hpp>

#include <allio/impl/error_encoding.hpp>
#include <allio/impl/linux/api_string.hpp>
#include <allio/impl/linux/error.hpp>
#include <allio/impl/linux/fcntl.hpp>
#include <allio/impl/linux/fork_exec.hpp>
#include <allio/impl/linux/handles/fs_object.hpp>
#include <allio/impl/linux/pidfd.hpp>
#include <allio/impl/linux/poll.hpp>
#include <allio/impl/linux/proc.hpp>
#include <allio/impl/linux/process_reaper.hpp>
#include <allio/impl/linux/process.hpp>

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
		return vsm::unexpected(allio_error(error::process_id_not_available));
	}

	// Linux PIDs are in the range [1, 2^22] and further limited
	// to at most 2^31-1 by the 32-bit integers used to store them.
	// Despite the kernel using long long for printing, there is no
	// reasonable scenario where pid would exceed the range of pid_t.
	vsm_assert(0 < pid && pid < std::numeric_limits<pid_t>::max());

	return static_cast<pid_t>(pid);
}
#endif


static constexpr process_exit_code no_exit_code = -1;

std::optional<process_exit_code> process_wait_result::_get(process_exit_code const exit_code)
{
	std::optional<process_exit_code> r;

	if (exit_code != no_exit_code)
	{
		r = exit_code;
	}

	return r;
}


vsm::result<void> process_t::open(
	native_handle<process_t>& h,
	io_parameters_t<process_t, open_t> const& a)
{
	vsm_try(pid, vsm::try_truncate<pid_t>(
		a.id.integer(),
		error::invalid_argument));

	vsm_try(fd, linux::pidfd_open(
		pid,
		/* flags: */ 0));

	if (vsm::any_flags(a.flags, io_flags::create_inheritable))
	{
		linux::set_inheritable(fd.get(), /* inheritable: */ true);
	}

	h.flags = flags::not_null;
	h.platform_handle = wrap_handle(fd.release());
	h.id = a.id;
	h.reaper = nullptr;

	return {};
}

vsm::result<void> process_t::create(
	native_handle<process_t>& h,
	io_parameters_t<process_t, create_t> const& a)
{
	//TODO: Implement inherit_handles

	process_reaper_ptr reaper;
	handle_flags h_flags = {};

	unique_handle pid_fd;
	pid_t pid = 0;

	// Launch the process using the internal fork_exec interface.
	{
		fork_exec_data data =
		{
			.exec_base = a.executable_path.base == nullptr
				? AT_FDCWD
				: unwrap_handle(a.executable_path.base->platform_handle),

			.inheritable_fd = vsm::any_flags(a.flags, io_flags::create_inheritable),
		};

		api_string_storage string_storage;
		std::string_view exec_path;
		std::string_view wdir_path;
		unique_handle exec_fd;

		bool const set_working_directory =
			a.working_directory.base != nullptr ||
			!a.working_directory.path.empty();

		vsm_try_void(api_string_builder::make(string_storage, [&](auto&& ctx)
		{
			exec_path = ctx.string(a.executable_path.path.string());

			if (!a.arguments.empty())
			{
				data.exec_argv = const_cast<char* const*>(ctx.strings(
					a.executable_path.path.string(),
					a.arguments));
			}

			if (vsm::any_flags(a.options, process_options::set_environment))
			{
				data.exec_envp = const_cast<char* const*>(ctx.strings(a.environment));
			}

			if (set_working_directory)
			{
				wdir_path = ctx.string(a.working_directory.path.string());
			}
		}));
		data.exec_path = exec_path.data();

		if (set_working_directory)
		{
			data.wdir_path = wdir_path.data();

			// If the executable path is relative, its meaning would change with the change of
			// working directory. In this case the file is opened ahead of time and the file
			// descriptor is passed to exec instead.
			if (a.executable_path.base == nullptr && path_view(exec_path).is_relative())
			{
				vsm_try_assign(exec_fd, linux::open_file(
					/* dir_fd: */ -1,
					exec_path.data(),
					O_PATH | O_CLOEXEC));

				data.exec_base = exec_fd.get();
				data.exec_path = "";
				data.exec_flags |= AT_EMPTY_PATH;
			}

			// If a working directory base handle is specified, make use of it only if the path is
			// relative. If the path is absolute, thee is no need for the base file descriptor.
			if (a.working_directory.base != nullptr && path_view(wdir_path).is_relative())
			{
				data.wdir_base = unwrap_handle(a.working_directory.base->platform_handle);
			}
		}

		if (a.redirect_stdin != nullptr)
		{
			data.exec_stdin = unwrap_handle(a.redirect_stdin->platform_handle);
		}
		if (a.redirect_stdout != nullptr)
		{
			data.exec_stdout = unwrap_handle(a.redirect_stdout->platform_handle);
		}
		if (a.redirect_stderr != nullptr)
		{
			data.exec_stderr = unwrap_handle(a.redirect_stderr->platform_handle);
		}

		// If the process is not launched detached and this handle does not wait on close, then a
		// process reaper object must be created to wait upon the child process.
		if (vsm::no_flags(
			a.options,
			process_options::launch_detached | process_options::wait_on_close))
		{
			// A duplicate fd is required for the reaper.
			data.duplicate_fd = true;

			// Create the reaper before launching to avoid the failure after the child process was
			// already launched, at which point we would have to terminate it and wait for exit.
			vsm_try_assign(reaper, acquire_process_reaper());
		}

		// Actually create the process:
		if (int const error = fork_exec(data))
		{
			return vsm::unexpected(allio_error(static_cast<system_error>(error)));
		}

		// Take ownership of the returned pid_fd.
		pid_fd.reset(data.pid_fd);

		if (reaper != nullptr)
		{
			// If a reaper is used, pass ownership of the duplicate pid_fd to it.
			start_process_reaper(reaper.get(), data.dup_fd);
		}
		else
		{
			vsm_assert(data.dup_fd == -1);
		}

		pid = data.pid;
	}

	if (vsm::any_flags(a.options, process_options::wait_on_close))
	{
		h_flags |= flags::wait_on_close;
	}

	h = native_handle<process_t>
	{
		native_handle<platform_object_t>
		{
			native_handle<object_t>
			{
				flags::not_null | h_flags,
			},
			wrap_handle(pid_fd.release()),
		},
		process_id(static_cast<process_id::integer_type>(pid)),
		reaper.release(),
	};

	return {};
}

vsm::result<void> process_t::terminate(
	native_handle<process_t> const& h,
	io_parameters_t<process_t, terminate_t> const& a)
{
	if (a.exit_code)
	{
		//TODO: Deal with the requested exit code somehow?
		return vsm::unexpected(allio_error(error::unsupported_operation));
	}

	if (pidfd_send_signal(
		unwrap_handle(h.platform_handle),
		SIGKILL,
		/* siginfo: */ nullptr,
		/* flags: */ 0) == -1)
	{
		// The process might have exited and been reaped already. This is not considered an error.
		if (int const e = errno; e != ESRCH)
		{
			return vsm::unexpected(allio_error(static_cast<system_error>(e)));
		}
	}

	return {};
}

vsm::result<process_wait_result> process_t::wait(
	native_handle<process_t> const& h,
	io_parameters_t<process_t, wait_t> const& a)
{
	if (static_cast<pid_t>(h.id.integer()) == getpid())
	{
		return vsm::unexpected(allio_error(error::process_is_current_process));
	}

	int const fd = unwrap_handle(h.platform_handle);

	if (h.reaper != nullptr)
	{
		// If a reaper is available, it must be used in order to store the exit code for any
		// subsequent calls to wait.
		vsm_try(exit_code, process_reaper_wait(h.reaper, fd, a.deadline));
		return process_wait_result(exit_code.value_or(no_exit_code));
	}
	else
	{
		// Otherwise, the process must be waited directly. The assumed child-process is not reaped
		// here, because a handle to it will still exist and in order to keep the exit code
		// available for subsequent calls to wait. The child-process will either be reaped at close
		// due to wait_on_close, or this handle was constructed manually by the user without neither
		// a reaper or wait_on_close. In this case the user has taken the responsibility of reaping
		// the child-process or either letting it zombify.
		vsm_try(exit_code, linux::wait_process(
			unwrap_handle(h.platform_handle),
			/* reap: */ false,
			a.deadline));

		return process_wait_result(exit_code.value_or(no_exit_code));
	}
}

vsm::result<void> process_t::close(
	native_handle<process_t>& h,
	io_parameters_t<process_t, close_t> const& a)
{
	if (h.flags[process_t::flags::wait_on_close])
	{
		if (static_cast<pid_t>(h.id.integer()) == getpid())
		{
			return vsm::unexpected(allio_error(error::process_is_current_process));
		}

		// Wait for the process to exit and reap it if possible. If the process is a non-child
		// process, requesting for it to be reaped does not return an error. Since the handle is
		// being closed anyway, the exit code can be discarded.
		vsm_try_discard(linux::wait_process(
			unwrap_handle(h.platform_handle),
			/* reap: */ true,
			deadline::never()));
	}

	// Close the underlying platform handle.
	vsm_try_void(base_type::close(h, a));

	// Only once the platform handle has been successfully closed, is the process reaper released.
	if (h.reaper != nullptr)
	{
		release_process_reaper(h.reaper);
		h.reaper = nullptr;
	}

	h.id = {};

	return {};
}


process_id _this_process::get_id() noexcept
{
	return process_id(static_cast<process_id::integer_type>(getpid()));
}

#if 0
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
#endif
