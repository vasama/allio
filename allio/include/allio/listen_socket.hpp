#pragma once

#include <allio/default_multiplexer.hpp>
#include <allio/handles/listen_socket.hpp>

namespace allio::async {

using listen_handle = basic_listen_handle<default_multiplexer_handle>;
using raw_listen_handle = basic_raw_listen_handle<default_multiplexer_handle>;

} // namespace allio::async
