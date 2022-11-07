#include <allio/impl/win32/platform_handle.hpp>

#include <allio/impl/win32/error.hpp>

#include <win32.h>

using namespace allio;
using namespace allio::win32;

result<void> platform_handle::close_sync(basic_parameters const& args)
{
	allio_ASSERT(m_native_handle.value != native_platform_handle::null);
	allio_TRYV(close_handle(unwrap_handle(m_native_handle.value)));
	m_native_handle = native_platform_handle::null;
	return {};
}

handle_flags win32::set_multiplexable_completion_modes(HANDLE const handle)
{
	if (SetFileCompletionNotificationModes(handle,
		FILE_SKIP_COMPLETION_PORT_ON_SUCCESS | FILE_SKIP_SET_EVENT_ON_HANDLE))
	{
		return
			handle_flags(platform_handle::implementation::flags::skip_completion_port_on_success) |
			handle_flags(platform_handle::implementation::flags::skip_handle_event_on_completion);
	}
	return handle_flags::none;
}

result<unique_handle> win32::duplicate_handle(HANDLE const handle)
{
	HANDLE const process = GetCurrentProcess();

	HANDLE duplicate;
	if (!DuplicateHandle(process, handle, process, &duplicate, 0, false, DUPLICATE_SAME_ACCESS))
	{
		return allio_ERROR(get_last_error());
	}

	return result<unique_handle>(result_value, duplicate);
}
