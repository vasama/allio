#pragma once

#include <allio/handles/raw_listen_socket.hpp>
#include <allio/senders.hpp>

namespace allio::senders {
inline namespace sockets {

template<detail::multiplexer_handle_for<raw_listen_socket_t> MultiplexerHandle>
using basic_raw_listen_socket_handle = traits_type::handle<raw_listen_socket_t, MultiplexerHandle>;

[[nodiscard]] detail::ex::sender auto raw_listen(network_endpoint const& endpoint, auto&&... args)
{
	return detail::listen<raw_listen_socket_t, traits_type>(endpoint, vsm_forward(args)...);
}

} // inline namespace sockets
} // namespace allio::senders
