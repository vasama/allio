#pragma once

#include <allio/default_multiplexer.hpp>
#include <allio/handles/socket_handle.hpp>

namespace allio {

allio_detail_export
template<typename Multiplexer = default_multiplexer_handle>
using socket_handle = detail::basic_handle<socket_handle_t, Multiplexer>;

} // namespace allio
