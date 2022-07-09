#pragma once

#include <allio/deadline.hpp>
#include <allio/detail/handles/event.hpp>

namespace allio {

using detail::event_options;
using detail::event_mode;
using detail::manual_reset_event;
using detail::auto_reset_event;
using detail::initially_signaled;
using detail::event_t;

} // namespace allio
