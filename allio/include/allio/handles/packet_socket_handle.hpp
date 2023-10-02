#pragma once

#include <allio/detail/handles/packet_socket_handle.hpp>

namespace allio {

using packet_socket_handle = basic_blocking_handle<detail::_packet_socket_handle>;

template<typename Multiplexer>
using basic_packet_socket_handle = basic_async_handle<detail::_packet_socket_handle, Multiplexer>;


} // namespace allio
