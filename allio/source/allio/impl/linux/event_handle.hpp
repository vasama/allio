#pragma once

#include <allio/event_handle.hpp>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

vsm::result<void> reset_event(int const fd);

} // namespace allio::linux

#include <allio/linux/detail/undef.i>
