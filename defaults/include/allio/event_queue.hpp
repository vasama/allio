#pragma once

#include <allio/default_multiplexer.hpp>
#include <allio/detail/event_queue.hpp>

namespace allio {

using detail::basic_event_queue;

using event_queue = basic_event_queue<default_multiplexer_ptr>;

} // namespace allio
