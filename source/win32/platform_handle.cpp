#include "platform_handle.hpp"

#include "error.hpp"

#include <win32.h>

using namespace allio;
using namespace allio::win32;

result<void> win32::close_handle(HANDLE const handle)
{
	if (!CloseHandle(handle))
	{
		return allio_ERROR(get_last_error_code());
	}
	return {};
}

result<void> platform_handle::do_close_sync()
{
	allio_ASSERT(m_native_handle.value != native_platform_handle::null);
	allio_TRYV(close_handle(unwrap_handle(m_native_handle.value)));
	m_native_handle = native_platform_handle::null;
	return {};
}
