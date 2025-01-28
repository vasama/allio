#pragma once

#include <allio/linux/detail/io_uring/multiplexer.hpp>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

namespace io_uring = detail::io_uring;

using detail::io_uring_multiplexer;

} // namespace allio::linux

#include <allio/linux/detail/undef.i>
