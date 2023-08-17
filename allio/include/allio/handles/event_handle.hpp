#pragma once

#include <allio/detail/handles/event_handle.hpp>

namespace allio {

using detail::event_handle;

template<typename Multiplexer>
using basic_event_handle = basic_async_handle<detail::_event_handle, Multiplexer>;

template<parameters<event_handle::create_parameters> P = event_handle::create_parameters::interface>
vsm::result<event_handle> create_event(event_handle::reset_mode const reset_mode, P const& args = {});

} // namespace allio
