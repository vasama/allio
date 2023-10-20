#pragma once

#include <allio/detail/handles/event_handle.hpp>

namespace allio {

using detail::event_reset_mode;
using detail::manual_reset_event;
using detail::auto_reset_event;

using detail::signal_event_t;
using detail::signal_event;

using detail::event_handle_t;
using detail::abstract_event_handle;

template<typename MultiplexerHandle>
using async_event_handle = detail::async_handle<event_handle_t, MultiplexerHandle>;

using detail::create_event;

} // namespace allio
