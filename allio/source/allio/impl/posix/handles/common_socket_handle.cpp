#include <allio/detail/handles/common_socket_handle.hpp>

#include <allio/impl/posix/socket.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::posix;

void common_socket_handle::close()
{
	auto const native_handle = get_platform_handle();
	vsm_assert(native_handle != native_platform_handle::null);
	vsm_verify(posix::close_socket(unwrap_socket(native_handle)));
}
