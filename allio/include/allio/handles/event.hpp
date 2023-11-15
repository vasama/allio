#pragma once

#include <allio/detail/handles/event.hpp>

namespace allio {

using detail::event_mode;
using detail::manual_reset_event;
using detail::auto_reset_event;

using detail::signal_event_t;
using detail::signal_event;

using detail::event_t;

using detail::abstract_event_handle;

namespace blocking { using namespace detail::_event_blocking; }
namespace async { using namespace detail::_event_async; }

} // namespace allio
