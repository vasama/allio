#pragma once

#include <allio/handles/listen_socket.hpp>
#include <allio/senders.hpp>

namespace allio::senders {
inline namespace listen_socket {

template<detail::multiplexer_handle_for<listen_socket_t> MultiplexerHandle>
using basic_listen_socket_handle = detail::sender_handle<listen_socket_t, MultiplexerHandle>;

template<detail::multiplexer_handle_for<raw_listen_socket_t> MultiplexerHandle>
using basic_raw_listen_socket_handle = detail::sender_handle<raw_listen_socket_t, MultiplexerHandle>;

#include <allio/detail/handles/listen_socket_io.ipp>

} // inline namespace listen_socket
} // namespace allio::senders
