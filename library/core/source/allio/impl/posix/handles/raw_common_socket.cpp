#include <allio/impl/posix/handles/raw_common_socket.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::posix;

network_address_kind raw_common_socket_base_t::get_address_kind(
	native_handle<raw_common_socket_base_t> const& h)
{
	return posix::get_address_kind(posix::get_address_family(h.flags));
}
