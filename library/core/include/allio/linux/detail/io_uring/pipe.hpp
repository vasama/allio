#pragma once

#include <allio/detail/handles/pipe.hpp>
#include <allio/linux/detail/io_uring/multiplexer.hpp>

#include <allio/linux/detail/io_uring/byte_io.hpp>

namespace allio::detail {

template<>
struct async_connector<io_uring_multiplexer, pipe_t>
	: io_uring_multiplexer::connector_type
{
};

template<>
struct async_operation<io_uring_multiplexer, pipe_t, byte_io::stream_read_t>
	: io_uring_byte_io_state<pipe_t, byte_io::stream_read_t>
{
};

template<>
struct async_operation<io_uring_multiplexer, pipe_t, byte_io::stream_write_t>
	: io_uring_byte_io_state<pipe_t, byte_io::stream_write_t>
{
};

} // namespace allio::detail
