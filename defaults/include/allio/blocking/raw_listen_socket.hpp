#pragma once

#include <allio/blocking.hpp>
#include <allio/handles/raw_listen_socket.hpp>

namespace allio::blocking {
inline namespace sockets {

using raw_listen_socket_handle = traits_type::handle<raw_listen_socket_t>;

[[nodiscard]] raw_listen_socket_handle raw_listen(network_endpoint const& endpoint, auto&&... args)
{
	return detail::listen<raw_listen_socket_t, traits_type>(endpoint, vsm_forward(args)...);
}

} // inline namespace sockets
} // namespace allio::blocking
