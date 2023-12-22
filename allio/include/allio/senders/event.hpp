#pragma once

#include <allio/handles/event.hpp>
#include <allio/senders.hpp>

namespace allio::senders {
inline namespace events {

template<detail::multiplexer_handle_for<event_t> MultiplexerHandle>
using basic_event_handle = traits_type::handle<event_t, MultiplexerHandle>;

[[nodiscard]] detail::ex::sender auto event(event_mode const mode, auto&&... args)
{
	return detail::create_event<traits_type>(mode, vsm_forward(args)...);
}

} // inline namespace events
} // namespace allio::senders
