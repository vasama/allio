#pragma once

#include <allio/blocking.hpp>
#include <allio/handles/event.hpp>

namespace allio::blocking {
inline namespace events {

using event_handle = traits_type::handle<event_t>;

[[nodiscard]] event_handle event(event_mode const mode, auto&&... args)
{
	return detail::create_event<traits_type>(mode, vsm_forward(args)...);
}

} // inline namespace events
} // namespace allio::blocking
