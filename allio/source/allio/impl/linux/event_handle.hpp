#pragma once

#include <allio/event_handle.hpp>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

vsm::result<bool> reset_event(int fd);

} // namespace allio::linux

#include <allio/linux/detail/undef.i>
