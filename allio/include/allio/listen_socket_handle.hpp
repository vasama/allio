#pragma once

#include <allio/default_multiplexer.hpp>
#include <allio/handles/listen_socket_handle.hpp>

namespace allio {

allio_detail_export
using async_listen_socket_handle = basic_listen_socket_handle<default_multiplexer>;

} // namespace allio
