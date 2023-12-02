#pragma once

#include <allio/deadline.hpp>
#include <allio/detail/handles/event.hpp>

namespace allio {

using detail::event_mode;
using detail::manual_reset_event;
using detail::auto_reset_event;

using detail::initially_signaled_t;
using detail::initially_signaled;

using detail::event_t;

using detail::abstract_event_handle;

namespace blocking { using namespace detail::_event_b; }
namespace async { using namespace detail::_event_a; }

} // namespace allio
