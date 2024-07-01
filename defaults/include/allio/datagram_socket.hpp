#pragma once

#include <allio/default_multiplexer.hpp>
#include <allio/senders/datagram_socket.hpp>

namespace allio {

using namespace senders::datagram_socket;

using datagram_socket_handle = basic_datagram_socket_handle<default_multiplexer_handle>;
using raw_datagram_socket_handle = basic_raw_datagram_socket_handle<default_multiplexer_handle>;

} // namespace allio
