#include <allio/impl/linux/platform_handle.hpp>

#include <allio/impl/linux/error.hpp>
#include <allio/linux/detail/unique_fd.hpp>

#include <unistd.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

vsm::result<void> platform_handle::close_sync(basic_parameters const& args)
{
	vsm_assert(m_native_handle.value != native_platform_handle::null);
	vsm_try_void(close_fd(unwrap_handle(m_native_handle.value)));
	m_native_handle = native_platform_handle::null;
	return {};
}
