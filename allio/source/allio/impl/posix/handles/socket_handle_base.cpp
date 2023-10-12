#include <allio/detail/handles/socket_handle_base.hpp>

#include <allio/impl/posix/socket.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::posix;

void detail::close_socket_handle(native_platform_handle const handle)
{
	vsm_assert(handle != native_platform_handle::null);
	vsm_verify(posix::close_socket(unwrap_socket(handle)));
}
