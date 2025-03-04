#pragma once

#include <allio/nothrow/traits.hpp>
#include <allio/handles/event.hpp>

namespace allio::nothrow {
inline namespace events {

using event_handle = traits_type::handle<event_t>;

[[nodiscard]] vsm::result<event_handle> event(event_mode const mode, auto&&... args)
{
	return detail::create_event<traits_type>(mode, vsm_forward(args)...);
}

} // inline namespace events
} // namespace allio::nothrow
