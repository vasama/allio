#pragma once

#include <allio/handles/event_handle.hpp>
#include <allio/linux/io_uring/multiplexer.hpp>

namespace allio {

template<>
struct async_handle_traits<linux::io_uring_multiplexer, event_handle::base_type>
{
	struct context_type
	{
	};
};

template<>
struct async_operation_traits<linux::io_uring_multiplexer, event_handle::base_type, event_handle::wait_tag>
{
	struct storage_type
	{
		/// @brief Storage for the output of the read from the eventfd.
		uint64_t event_value;

		/// @brief Timeout passed by reference in a linked timeout SQE.
		linux::io_uring_multiplexer::timeout timeout;
	};
};

} // namespace allio
