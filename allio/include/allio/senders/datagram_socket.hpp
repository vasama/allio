#pragma once

#include <allio/handles/datagram_socket.hpp>
#include <allio/senders.hpp>

namespace allio::senders {
inline namespace datagram_socket {

template<detail::multiplexer_handle_for<datagram_socket_t> MultiplexerHandle>
using basic_datagram_socket_handle = detail::sender_handle<datagram_socket_t, MultiplexerHandle>;

template<detail::multiplexer_handle_for<raw_datagram_socket_t> MultiplexerHandle>
using basic_raw_datagram_socket_handle = detail::sender_handle<raw_datagram_socket_t, MultiplexerHandle>;

#include <allio/detail/handles/datagram_socket_io.ipp>

} // inline namespace datagram_socket
} // namespace allio::senders
