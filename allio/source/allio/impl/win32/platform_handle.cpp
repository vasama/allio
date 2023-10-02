#include <allio/impl/win32/platform_handle.hpp>

#include <allio/impl/win32/error.hpp>

#include <win32.h>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

handle_flags win32::set_multiplexable_completion_modes(HANDLE const handle)
{
	if (SetFileCompletionNotificationModes(
		handle,
		FILE_SKIP_COMPLETION_PORT_ON_SUCCESS | FILE_SKIP_SET_EVENT_ON_HANDLE))
	{
		return
			handle_flags(platform_handle::implementation::flags::skip_completion_port_on_success) |
			handle_flags(platform_handle::implementation::flags::skip_handle_event_on_completion);
	}
	return handle_flags::none;
}

vsm::result<unique_handle> win32::duplicate_handle(HANDLE const handle)
{
	HANDLE const process = GetCurrentProcess();

	HANDLE duplicate;
	if (!DuplicateHandle(process, handle, process, &duplicate, 0, false, DUPLICATE_SAME_ACCESS))
	{
		return vsm::unexpected(get_last_error());
	}

	return vsm::result<unique_handle>(vsm::result_value, duplicate);
}


vsm::result<void> platform_handle::sync_impl(io::parameters_with_result<io::close> const& args)
{
	platform_handle& h = static_cast<platform_handle&>(*args.handle);
	vsm_assert(h);
	
	if (h.m_native_handle.value != native_platform_handle::null)
	{
		vsm_try_void(close_handle(unwrap_handle(h.m_native_handle.value)));
		h.m_native_handle.reset();
	}

	unrecoverable(base_type::sync_impl(args));

	return {};
}

#if 0
vsm::result<void> platform_handle::close_sync(basic_parameters const& args)
{
	vsm_assert(m_native_handle.value != native_platform_handle::null);
	vsm_try_void(close_handle(unwrap_handle(m_native_handle.value)));
	m_native_handle = native_platform_handle::null;
	return {};
}
#endif

vsm::result<native_platform_handle> platform_handle::duplicate(native_platform_handle const handle)
{
	vsm_try(new_handle, duplicate_handle(unwrap_handle(handle)));
	return wrap_handle(new_handle.release());
}
