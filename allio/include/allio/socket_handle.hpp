#pragma once

#include <allio/default_multiplexer.hpp>
#include <allio/handles/socket_handle.hpp>

namespace allio {

allio_detail_export
template<typename Multiplexer = default_multiplexer_handle>
using socket_handle = basic_socket_handle<Multiplexer>;

} // namespace allio
