#include <allio/detail/unique_socket.hpp>

#include <allio/impl/posix/socket.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::posix;

void detail::close_wrapped_socket(native_platform_handle const handle)
{
	vsm_assert(handle != native_platform_handle::null);
	posix::close_socket(unwrap_socket(handle));
}
