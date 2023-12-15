#pragma once

#include <allio/blocking.hpp>
#include <allio/handles/event.hpp>

namespace allio::blocking {
inline namespace event {

using event_handle = detail::blocking_handle<event_t>;

#include <allio/detail/handles/event_io.ipp>

} // inline namespace event
} // namespace allio::blocking
