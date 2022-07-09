#include "platform_handle.hpp"

#include "error.hpp"

#include <unistd.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::linux;

result<void> platform_handle::do_close_sync()
{
	allio_ASSERT(m_native_handle.value != native_platform_handle::null);
	if (::close(unwrap_handle(m_native_handle.value)) == -1)
	{
		return allio_ERROR(get_last_error_code());
	}
	m_native_handle = native_platform_handle::null;
	return {};
}
