#pragma once

#include <allio/handles/opaque_object.hpp>
#include <allio/linux/io_uring/multiplexer.hpp>

namespace allio::detail {

template<>
struct async_connector<io_uring_multiplexer, opaque_object_t>
{
};

template<>
struct async_operation<io_uring_multiplexer, opaque_object_t, opaque_object_t::poll_t>
{
};

} // namespace allio::detail
