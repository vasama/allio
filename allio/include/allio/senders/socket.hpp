#pragma once

#include <allio/handles/socket.hpp>
#include <allio/senders.hpp>

namespace allio::senders {
inline namespace socket {

template<detail::multiplexer_handle_for<socket_t> MultiplexerHandle>
using basic_socket_handle = detail::sender_handle<socket_t, MultiplexerHandle>;

template<detail::multiplexer_handle_for<raw_socket_t> MultiplexerHandle>
using basic_raw_socket_handle = detail::sender_handle<raw_socket_t, MultiplexerHandle>;

#include <allio/detail/handles/socket_io.ipp>

} // inline namespace socket
} // namespace allio::senders
