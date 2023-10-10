#pragma once

#include <allio/default_multiplexer.hpp>
#include <allio/handles/event_handle.hpp>

namespace allio {

allio_detail_export
using event_handle = basic_event_handle<default_multiplexer_ptr>;

} // namespace allio
