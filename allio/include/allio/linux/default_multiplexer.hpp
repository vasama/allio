#pragma once

#include <allio/linux/io_uring_multiplexer.hpp>

#include <allio/linux/detail/undef.i>

#define allio_detail_default_multiplexer io_uring

namespace allio {

using default_multiplexer = linux::io_uring_multiplexer;

} // namespace allio

#include <allio/linux/detail/undef.i>
