#pragma once

#include <allio/handles/opaque_handle.hpp>
#include <allio/linux/io_uring/multiplexer.hpp>

namespace allio {

template<>
struct async_handle_traits<linux::io_uring_multiplexer, opaque_handle::base_type>
{
	struct context_type
	{
	};
};

template<>
struct async_operation_traits<linux::io_uring_multiplexer, opaque_handle::base_type, opaque_handle::poll_tag>
{
	struct storage_type
	{
		/// @brief Timeout passed by reference in a linked timeout SQE.
		linux::io_uring_multiplexer::timeout timeout;
	};
};

} // namespace allio
