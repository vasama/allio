#pragma once

#include <allio/default_multiplexer.hpp>
#include <allio/handles/socket.hpp>

namespace allio::async {

using socket_handle = basic_socket_handle<default_multiplexer_handle>;
using raw_socket_handle = basic_raw_socket_handle<default_multiplexer_handle>;

} // namespace allio::async
