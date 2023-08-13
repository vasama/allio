#include <allio/platform.hpp>

#include <allio/linux/detail/unique_fd.hpp>
#include <allio/linux/platform.hpp>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

vsm::result<void> detail::close_native_platform_handle(native_platform_handle const handle) noexcept
{
	return close_fd(unwrap_handle(handle));
}
