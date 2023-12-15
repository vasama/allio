#pragma once

#include <allio/blocking.hpp>
#include <allio/handles/datagram_socket.hpp>

namespace allio::blocking {
inline namespace datagram_socket {

using datagram_socket_handle = detail::blocking_handle<datagram_socket_t>;
using raw_datagram_socket_handle = detail::blocking_handle<raw_datagram_socket_t>;

#include <allio/detail/handles/datagram_socket_io.ipp>

} // inline namespace datagram_socket
} // namespace allio::blocking
