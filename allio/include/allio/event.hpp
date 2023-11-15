#pragma once

#include <allio/default_multiplexer.hpp>
#include <allio/handles/event.hpp>

namespace allio::async {

using event_handle = basic_event_handle<default_multiplexer_handle>;

} // namespace allio::async
