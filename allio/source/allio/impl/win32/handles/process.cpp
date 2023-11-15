#include <allio/impl/win32/handles/process.hpp>

#include <allio/detail/dynamic_buffer.hpp>
#include <allio/impl/transcode.hpp>
#include <allio/impl/win32/command_line.hpp>
#include <allio/impl/win32/error.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/impl/win32/kernel_path.hpp>
#include <allio/win32/kernel_error.hpp>
#include <allio/win32/platform.hpp>

#include <vsm/lazy.hpp>
#include <vsm/utility.hpp>

#include <algorithm>
#include <memory>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;



vsm::result<unique_handle> win32::open_process(process_id const pid)
{
	HANDLE const handle = OpenProcess(
		SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION,
		/* bInheritHandle: */ false,
		pid);

	if (!handle)
	{
		return vsm::unexpected(get_last_error());
	}

	return vsm_lazy(unique_handle(handle));
}

vsm::result<process_info> win32::launch_process(
	path_descriptor const path,
	any_string_span const arguments,
	any_string_span const environment,
	path_descriptor const* const working_directory)
{
	if (path.base != nullptr)
	{
		return vsm::unexpected(error::unsupported_operation);
	}

	kernel_path_storage path_storage;
	vsm_try(kernel_path, make_kernel_path(path_storage,
	{
		.path = path.path,
		.win32_api_form = true,
	}));

	command_line_storage command_line_storage;
	vsm_try(command_line, make_command_line(
		command_line_storage,
		path.path.string(),
		arguments));

	DWORD creation_flags = 0;

	STARTUPINFOW startup_info = {};

	PROCESS_INFORMATION process_info;

	if (!CreateProcessW(
		kernel_path.path.data(),
		command_line,
		/* lpProcessAttributes: */ nullptr,
		/* lpThreadAttributes: */ nullptr,
		/* bInheritHandles: */ false,
		creation_flags,
		/* lpEnvironment: */ nullptr, //TODO: Set environment variables
		/* lpCurrentDirectory: */ nullptr, //TODO: Set current directory
		&startup_info,
		&process_info))
	{
		return vsm::unexpected(get_last_error());
	}

	// Automatically close the thread handle on return.
	unique_handle const thread_handle(process_info.hThread);

	return vsm_lazy(win32::process_info
	{
		.handle = unique_handle(process_info.hProcess),
		.pid = static_cast<process_id>(process_info.dwProcessId),
	});
}

vsm::result<process_exit_code> win32::get_process_exit_code(HANDLE const handle)
{
	DWORD exit_code;
	if (!GetExitCodeProcess(handle, &exit_code))
	{
		return vsm::unexpected(get_last_error());
	}
	return static_cast<int32_t>(exit_code);
}


//TODO: Override close to handle pseudo handles.

vsm::result<process_id> process_handle_t::get_process_id() const
{
	DWORD const id = GetProcessId(unwrap_handle(get_platform_handle()));

	if (id == 0)
	{
		return vsm::unexpected(get_last_error());
	}

	return static_cast<process_id>(id);
}


vsm::result<void> process_handle_t::do_blocking_io(
	process_handle_t& h,
	io_result_ref_t<open_t> const,
	io_parameters_t<open_t> const& args)
{
	vsm_try_void(kernel_init());

	if (h)
	{
		return vsm::unexpected(error::handle_is_not_null);
	}

	vsm_try(handle, win32::open_process(args.process_id));

	handle_flags flags = flags::none;

	if (static_cast<DWORD>(args.process_id) == GetCurrentProcessId())
	{
		flags |= flags::current;
	}

	platform_handle::native_handle_type const native =
	{
		{
			flags::not_null | flags,
		},
		wrap_handle(handle.get()),
	};

	vsm_assert(check_native_handle(native));
	h.set_native_handle(native);

	(void)handle.release();

	return {};
}

vsm::result<void> process_handle_t::do_blocking_io(
	process_handle_t& h,
	io_result_ref_t<launch_t> const result,
	io_parameters_t<launch_t> const& args)
{
	vsm_try_void(kernel_init());

	if (h)
	{
		return vsm::unexpected(error::handle_is_not_null);
	}

	vsm_try_bind((handle, process_id), win32::launch_process(
		args.path,
		args.command_line,
		args.environment,
		args.working_directory ? &*args.working_directory : nullptr));

	platform_handle::native_handle_type const native =
	{
		{
			flags::not_null,
		},
		wrap_handle(handle.get()),
	};

	vsm_assert(check_native_handle(native));
	h.set_native_handle(native);
	(void)handle.release();

	return {};
}

vsm::result<void> process_handle_t::do_blocking_io(
	process_handle_t const& h,
	io_result_ref_t<wait_t> const result,
	io_parameters_t<wait_t> const& args)
{
	if (!h)
	{
		return vsm::unexpected(error::handle_is_null);
	}

	if (h.is_current())
	{
		return vsm::unexpected(error::process_is_current_process);
	}

	HANDLE const handle = unwrap_handle(h.get_platform_handle());

	NTSTATUS const status = win32::NtWaitForSingleObject(
		handle,
		/* Alertable: */ false,
		kernel_timeout(args.deadline));

	if (status == STATUS_TIMEOUT)
	{
		return vsm::unexpected(error::async_operation_timed_out);
	}

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return result.produce(get_process_exit_code(handle));
}


basic_process_handle<void> const& detail::_this_process::get_handle()
{
	static basic_process_handle<void> const handle = [&]()
	{
		basic_process_handle<void> h;
		vsm_verify(h.set_native_handle(platform_handle::native_handle_type
		{
			handle::native_handle_type
			{
				handle_flags(process_handle_t::flags::not_null)
					| process_handle_t::flags::current
					| process_handle_t::impl_type::flags::pseudo_handle,
			},
			wrap_handle(GetCurrentProcess()),
		}));
		return h;
	}();
	return handle;
}
