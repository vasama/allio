#pragma once

#include <allio/default_multiplexer.hpp>
#include <allio/senders/event.hpp>

namespace allio {

using namespace senders::events;

using event_handle = basic_event_handle<default_multiplexer_handle>;

} // namespace allio
