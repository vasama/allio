#pragma once

#include <allio/default_multiplexer.hpp>
#include <allio/handles/packet_socket_handle.hpp>

#include allio_detail_default_handle_include(packet_socket_handle)

namespace allio {

allio_detail_export
using async_packet_socket_handle = basic_packet_socket_handle<default_multiplexer>;

} // namespace allio
