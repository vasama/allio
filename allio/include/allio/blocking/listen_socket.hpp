#pragma once

#include <allio/blocking/socket.hpp>
#include <allio/handles/listen_socket.hpp>

namespace allio::blocking {
inline namespace socket {

using listen_socket_handle = detail::blocking_handle<listen_socket_t>;
using raw_listen_socket_handle = detail::blocking_handle<raw_listen_socket_t>;

#include <allio/detail/handles/listen_socket_io.ipp>

} // inline namespace socket
} // namespace allio::blocking
