#pragma once

#include <allio/default_multiplexer.hpp>
#include <allio/handles/event_handle.hpp>

namespace allio {

allio_detail_export
template<typename Multiplexer = default_multiplexer_handle>
using event_handle = detail::basic_handle<event_handle_t, Multiplexer>;

} // namespace allio
