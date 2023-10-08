#pragma once

#include <allio/default_multiplexer.hpp>
#include <allio/handles/stream_socket_handle.hpp>

namespace allio {

allio_detail_export
using stream_socket_handle = basic_stream_socket_handle<default_multiplexer_ptr>;

} // namespace allio
