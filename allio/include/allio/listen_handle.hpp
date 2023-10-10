#pragma once

#include <allio/default_multiplexer.hpp>
#include <allio/handles/listen_handle.hpp>

namespace allio {

allio_detail_export
template<typename Multiplexer = default_multiplexer_handle>
using listen_handle = basic_listen_handle<Multiplexer>;

} // namespace allio
