#pragma once

#include <allio/linux/io_uring_multiplexer.hpp>
#include <allio/queue_multiplexer.hpp>

#include <allio/linux/detail/undef.i>

namespace allio {

//TODO: default_multiplexer should not expose platform specific APIs.
using default_multiplexer = queue_multiplexer<linux::io_uring_multiplexer>;

} // namespace allio

#include <allio/linux/detail/undef.i>
