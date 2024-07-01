#pragma once

#include <allio/blocking.hpp>
#include <allio/handles/raw_datagram_socket.hpp>

namespace allio::blocking {
inline namespace datagram_sockets {

using raw_datagram_socket_handle = traits_type::handle<raw_datagram_socket_t>;

[[nodiscard]] raw_datagram_socket_handle raw_bind(network_endpoint const& endpoint, auto&&... args)
{
	return detail::bind<raw_datagram_socket_t, traits_type>(endpoint, vsm_forward(args)...);
}

} // inline namespace datagram_sockets
} // namespace allio::blocking
