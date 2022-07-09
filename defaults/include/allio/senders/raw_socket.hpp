#pragma once

#include <allio/handles/raw_socket.hpp>
#include <allio/senders.hpp>

namespace allio::senders {
inline namespace sockets {

template<detail::multiplexer_handle_for<raw_socket_t> MultiplexerHandle>
using basic_raw_socket_handle = traits_type::handle<raw_socket_t, MultiplexerHandle>;

[[nodiscard]] detail::ex::sender auto raw_connect(network_endpoint const& endpoint, auto&&... args)
{
	return detail::connect<raw_socket_t, traits_type>(endpoint, vsm_forward(args)...);
}

} // inline namespace sockets
} // namespace allio::senders
