#pragma once

#include <allio/handles/raw_datagram_socket.hpp>
#include <allio/senders.hpp>

namespace allio::senders {
inline namespace datagram_socket {

template<detail::multiplexer_handle_for<raw_datagram_socket_t> MultiplexerHandle>
using basic_raw_datagram_socket_handle = traits_type::handle<raw_datagram_socket_t, MultiplexerHandle>;

[[nodiscard]] detail::ex::sender auto raw_bind(network_endpoint const& endpoint, auto&&... args)
{
	return detail::bind<raw_datagram_socket_t, traits_type>(endpoint, vsm_forward(args)...);
}

} // inline namespace datagram_socket
} // namespace allio::senders
