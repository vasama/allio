#pragma once

#include <allio/default_multiplexer.hpp>
#include <allio/handles/event_handle.hpp>

namespace allio {

allio_detail_export
template<typename Multiplexer = default_multiplexer_handle>
using event_handle = basic_event_handle<Multiplexer>;

} // namespace allio
