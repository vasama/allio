#pragma once

#include <allio/detail/handles/event.hpp>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

inline bool is_auto_reset(detail::native_handle<detail::event_t> const& h)
{
	return h.flags[detail::event_t::flags::auto_reset];
}

vsm::result<bool> reset_event(int fd);
vsm::result<void> test_event(int fd, bool auto_reset);

} // namespace allio::linux

#include <allio/linux/detail/undef.i>
