#pragma once

#include <allio/blocking.hpp>
#include <allio/handles/socket.hpp>

namespace allio::blocking {
inline namespace socket {

using socket_handle = detail::blocking_handle<socket_t>;
using raw_socket_handle = detail::blocking_handle<raw_socket_t>;

#include <allio/detail/handles/socket_io.ipp>

} // inline namespace socket
} // namespace allio::blocking
