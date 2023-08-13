#include <allio/platform.hpp>

#include <allio/win32/detail/unique_handle.hpp>
#include <allio/win32/platform.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

vsm::result<void> detail::close_native_platform_handle(native_platform_handle const handle) noexcept
{
	return close_handle(unwrap_handle(handle));
}
