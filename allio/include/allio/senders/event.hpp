#pragma once

#include <allio/handles/event.hpp>
#include <allio/senders.hpp>

namespace allio::senders {
inline namespace event {

template<detail::multiplexer_handle_for<event_t> MultiplexerHandle>
using basic_event_handle = detail::sender_handle<event_t, MultiplexerHandle>;

#include <allio/detail/handles/event_io.ipp>

} // inline namespace event
} // namespace allio::senders
