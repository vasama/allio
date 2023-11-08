#pragma once

#include <allio/detail/handles/socket_handle.hpp>

namespace allio {

using detail::socket_handle_t;
using detail::abstract_socket_handle;
using detail::blocking_socket_handle;
using detail::async_socket_handle;

using detail::connect;
using detail::connect_blocking;

using detail::raw_socket_handle_t;
using detail::abstract_raw_socket_handle;
using detail::blocking_raw_socket_handle;
using detail::async_raw_socket_handle;

using detail::raw_connect;
using detail::raw_connect_blocking;

} // namespace allio
