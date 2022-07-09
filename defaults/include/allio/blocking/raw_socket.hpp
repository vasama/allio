#pragma once

#include <allio/blocking.hpp>
#include <allio/handles/raw_socket.hpp>

namespace allio::blocking {
inline namespace sockets {

using raw_socket_handle = traits_type::handle<raw_socket_t>;

[[nodiscard]] raw_socket_handle raw_connect(network_endpoint const& endpoint, auto&&... args)
{
	return detail::connect<raw_socket_t, traits_type>(endpoint, vsm_forward(args)...);
}

} // inline namespace sockets
} // namespace allio::blocking
