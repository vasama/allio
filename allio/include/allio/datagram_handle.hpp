#pragma once

#include <allio/default_multiplexer.hpp>
#include <allio/handles/datagram_handle.hpp>

namespace allio::async {

using datagram_socket_handle = basic_datagram_socket_handle<default_multiplexer_handle>;

} // namespace allio::async
