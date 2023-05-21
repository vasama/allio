#include <allio/impl/linux/platform_handle.hpp>

#include <allio/impl/linux/error.hpp>

#include <unistd.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::linux;

vsm::result<void> platform_handle::close_sync()
{
	vsm_assert(m_native_handle.value != native_platform_handle::null);
	if (::close(unwrap_handle(m_native_handle.value)) == -1)
	{
		return std::unexpected(get_last_error());
	}
	m_native_handle = native_platform_handle::null;
	return {};
}
