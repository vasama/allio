#pragma once

#include <allio/default_multiplexer.hpp>
#include <allio/senders/socket.hpp>

namespace allio {

using namespace senders::socket;

using socket_handle = basic_socket_handle<default_multiplexer_handle>;
using raw_socket_handle = basic_raw_socket_handle<default_multiplexer_handle>;

} // namespace allio
