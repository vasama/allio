#include <allio/handles/process.hpp>

#include <allio/detail/default_sequence_container.hpp>
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
#include <allio/impl/linux/readlink.hpp>
#include <allio/impl/linux/version.hpp>
#include <allio/impl/storage_provider.hpp>
#include <allio/impl/transcode.hpp>

#include <vsm/numeric.hpp>
#include <vsm/utility.hpp>

#include <algorithm>
#include <thread>
#include <vector>

#include <sys/wait.h>

#include <linux/limits.h>
#include <linux/wait.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

#if allio_config_sanitize
namespace allio::sanitizer {

static void check_inheritable_fd(int const fd)
{
	int const flags = ::fcntl(fd, F_GETFD);

	if (flags < 0)
	{
		vsm_assert(flags == EBADF);
		sanitizer::report_error("File descriptor is not valid: {}", wrap_handle(fd));
	}
	else if (flags & FD_CLOEXEC)
	{
		sanitizer::report_error("File descriptor is not inheritable (CLOEXEC): {}", wrap_handle(fd));
	}
}

} // namespace allio::sanitizer
#endif // allio_config_sanitize


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

	// Linux PIDs are in the range [1, 2^22] and further limited to at most 2^31-1 by the 32-bit
	// integers used to store them. Despite the kernel using long long for printing, there is no
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


template<size_t Capacity>
static vsm::result<void> make_inherit_fd_array(
	fork_exec_data& data,
	std::span<int const> const default_inherit_fd,
	platform_handles_view const user_handles,
	dynamic_storage_provider<Capacity>& storage_provider)
{
	using native_handle_type = native_handle<platform_object_t>;
	using pointer_type = native_handle_type const*;

	static constexpr size_t pointer_to_fd_ratio = sizeof(pointer_type) / sizeof(int);

	union user_handle_union
	{
		pointer_type pointer;

		struct
		{
			int fd[pointer_to_fd_ratio];
		};
	};

	static_assert(sizeof(user_handle_union) == sizeof(pointer_type));

	if (size_t const user_handle_count = user_handles.copy_to(nullptr))
	{
		size_t const handle_count = default_inherit_fd.size() + user_handle_count;

		vsm_try(storage, storage_provider.get_storage(
			handle_count * sizeof(user_handle_union),
			handle_count * sizeof(user_handle_union),
			std::align_val_t(alignof(user_handle_union))));

		auto const user_handle_pointer_array = vsm::start_lifetime_as_array<pointer_type>(
			storage.storage,
			user_handle_count);

		vsm_verify(user_handles.copy_to(user_handle_pointer_array) == user_handle_count);

		auto const user_handle_union_array = vsm::start_lifetime_as_array<user_handle_union>(
			storage.storage,
			user_handle_count);

		for (size_t i = 0; i < user_handle_count; ++i)
		{
			size_t const i_div = i / pointer_to_fd_ratio;
			size_t const i_mod = i % pointer_to_fd_ratio;

			user_handle_union& union_1 = user_handle_union_array[i];
			user_handle_union& union_n = user_handle_union_array[i_div];

			union_n.fd[i_mod] = unwrap_handle(union_1.pointer->platform_handle);
		}

		auto const user_fd_array = vsm::start_lifetime_as_array<int>(
			storage.storage,
			user_handle_count);

#if allio_config_sanitize
		for (int const fd : std::span(user_fd_array, user_handle_count))
		{
			sanitizer::check_inheritable_fd(fd);
		}
#endif // allio_config_sanitize

		std::memcpy(
			user_fd_array + user_handle_count,
			default_inherit_fd.data(),
			default_inherit_fd.size() * sizeof(int));

		data.inherit_fd_array = user_fd_array;
		data.inherit_fd_count = user_handle_count;

		std::sort(user_fd_array, user_fd_array + handle_count);
	}
	else
	{
		data.inherit_fd_array = default_inherit_fd.data();
		data.inherit_fd_count = default_inherit_fd.size();
	}

	return {};
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
	process_reaper_ptr reaper;
	handle_flags h_flags = {};

	unique_handle pid_fd;
	pid_t pid = 0;

	// Launch the process using the internal fork-exec interface.
	{
		fork_exec_data data =
		{
			.exec_base = a.executable_path.base == nullptr
				? AT_FDCWD
				: unwrap_handle(a.executable_path.base->platform_handle),

			.fork_detached = vsm::any_flags(a.options, process_options::launch_detached),
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

		static constexpr auto set_standard_stream = [](
			int& exec_fd,
			native_handle<platform_object_t> const* const p_handle)
		{
			if (p_handle != nullptr)
			{
				exec_fd = unwrap_handle(p_handle->platform_handle);

#if allio_config_sanitize
				// TODO: This is a portability issue. Only works on Linux, not on Windows.
				sanitizer::check_inheritable_fd(exec_fd);
#endif // allio_config_sanitize
			}
		};

		set_standard_stream(data.exec_stdin, a.redirect_stdin);
		set_standard_stream(data.exec_stdout, a.redirect_stdout);
		set_standard_stream(data.exec_stderr, a.redirect_stderr);

		static constexpr int default_inherit_fd[] =
		{
			STDIN_FILENO,
			STDOUT_FILENO,
			STDERR_FILENO,
		};

		dynamic_storage_provider<8 * sizeof(void*)> inherit_fd_storage;
		if (vsm::any_flags(a.options, process_options::inherit_handles))
		{
			if (a.inherit_handles)
			{
				vsm_try_void(make_inherit_fd_array(
					data,
					default_inherit_fd,
					a.inherit_handles,
					inherit_fd_storage));
			}
		}
		else
		{
			static_assert(std::ranges::is_sorted(default_inherit_fd));

			// Close all open file descriptors, except for the standard stream descriptors:
			data.inherit_fd_array = default_inherit_fd;
			data.inherit_fd_count = std::size(default_inherit_fd);
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
			// already launched, at which point failure to propagate the resulting handler to the
			// user is no longer an option.
			vsm_try_assign(reaper, acquire_process_reaper());
		}

		// Launch the child process using fork-exec:
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

vsm::result<native_platform_handle> process_t::duplicate_handle(
	native_handle<process_t> const& h,
	io_parameters_t<process_t, duplicate_handle_t> const& a)
{
	//TODO: Handle create_synchronized, create_non_blocking somehow?

	vsm_try(new_fd, linux::pidfd_getfd(
		/* fd: */ unwrap_handle(h.platform_handle),
		/* target_fd: */ unwrap_handle(a.platform_handle),
		/* flags: */ 0));

	if (vsm::any_flags(a.flags, io_flags::create_inheritable))
	{
		vsm_try_void(set_inheritable(new_fd.get(), /* inheritable: */ true));
	}

	return wrap_handle(new_fd.release());
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


static vsm::result<size_t> get_current_executable_path(string_buffer<char> const buffer)
{
	// https://www.man7.org/linux/man-pages/man5/proc_pid_exe.5.html

	if (get_kernel_version() < KERNEL_VERSION(2, 2, 0))
	{
		return vsm::unexpected(allio_error(error::unsupported_operation));
	}

	vsm_try(path_size, linux::read_link_path(/* dirfd: */ -1, "/proc/self/exe", buffer));
	auto const path = std::string_view(vsm::assume_success(buffer.resize(path_size)));

	if (path.ends_with(" (deleted)"))
	{
		// TODO: Handle this case better. At least provide a better error code.
		return vsm::unexpected(allio_error(error::unknown_failure));
	}

	return path.size();
}

template<typename Char>
static vsm::result<size_t> get_current_executable_path(string_buffer<Char> const buffer)
{
	default_sequence_container<char, 512> container;
	vsm_try(path_size, ::get_current_executable_path(container));
	auto const string = std::string_view(container.begin(), path_size);
	return transcode_string(string, buffer);
}

vsm::result<size_t> detail::get_current_executable_path(any_path_buffer const buffer)
{
	return buffer.string().visit([](auto const buffer)
	{
		return ::get_current_executable_path(buffer);
	});
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
