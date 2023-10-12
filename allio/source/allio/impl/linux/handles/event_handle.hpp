#pragma once

#include <allio/event_handle.hpp>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

inline bool is_auto_reset(detail::_event_handle const& h)
{
	return h.get_flags()[detail::_event_handle::flags::auto_reset];
}

vsm::result<void> poll_event(int fd, deadline deadline);
vsm::result<bool> reset_event(int fd);
vsm::result<void> test_event(int fd, bool auto_reset);

} // namespace allio::linux

#include <allio/linux/detail/undef.i>
