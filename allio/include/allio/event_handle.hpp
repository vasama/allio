#pragma once

#include <allio/default_multiplexer.hpp>
#include <allio/handles/event_handle.hpp>

#include allio_detail_default_handle_include(event_handle)

namespace allio {

allio_detail_export
using event_handle = basic_event_handle<default_multiplexer_ptr>;

} // namespace allio
