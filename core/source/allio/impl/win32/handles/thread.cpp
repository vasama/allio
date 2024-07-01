#include <allio/handles/thread.hpp>
#include <allio/impl/win32/handles/thread.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

vsm::result<thread_exit_code> win32::get_thread_exit_code(HANDLE const handle)
{
	DWORD exit_code;
	if (!GetExitCodeThread(handle, &exit_code))
	{
		return vsm::unexpected(get_last_error());
	}
	return static_cast<thread_exit_code>(exit_code);
}


vsm::result<void> thread_t::open(
	native_type& h,
	io_parameters_t<thread_t, open_t> const& a)
{
	vsm_try(handle, win32::open_thread(a.id));

	h = platform_object_t::native_type
	{
		{
			flags::not_null,
		},
		wrap_handle(handle.release()),
	};

	return {};
}

vsm::result<void> thread_t::terminate(
	native_type const& h,
	io_parameters_t<thread_t, terminate_t> const& a)
{
	NSTATATUS const status = NtTerminateThread(
		unwrap_handle(h.platform_handle),
		//TODO: Does this require some kind of transformation?
		static_cast<NTSTATUS>(a.exit_code));

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return {};
}

vsm::result<process_exit_code> thread_t::wait(
	native_type const& h,
	io_parameters_t<thread_t, wait_t> const& a)
{
	HANDLE const handle = unwrap_handle(h.platform_handle);

	NTSTATUS const status = win32::NtWaitForSingleObject(
		handle,
		/* Alertable: */ false,
		kernel_timeout(a.deadline));

	if (status == STATUS_TIMEOUT)
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return get_thread_exit_code(handle);
}


blocking::thread_handle const& this_thread::get_handle()
{
	using flags = thread_t::flags;
	using i_flags = thread_t::impl_type::flags;

	static thread_local blocking::thread_handle const handle
	(
		adopt_handle,
		platform_object_t::native_type
		{
			{
				handle_flags(flags::not_null)
					| i_flags::pseudo_handle,
			},
			wrap_handle(GetCurrentProcess()),
		}
	);
	return handle;
}

vsm::result<blocking::thread_handle> this_process::open()
{
	using flags = thread_t::flags;

	vsm_try(handle, duplicate_handle(GetCurrentProcess()));

	return vsm_lazy(blocking::thread_handle
	(
		adopt_handle,
		platform_object_t::native_type
		{
			{
				handle_flags(flags::not_null),
			},
			wrap_handle(handle.release()),
		}
	));
}
