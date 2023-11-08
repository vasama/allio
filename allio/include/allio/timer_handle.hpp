#pragma once

#include <allio/default_multiplexer.hpp>
#include <allio/handles/timer_handle.hpp>

namespace allio::async {

using timer_handle = basic_timer_handle<default_multiplexer_handle>;

} // namespace allio::async
