#pragma once

#include <allio/detail/handles/event_handle.hpp>
#include <allio/linux/detail/io_uring/multiplexer.hpp>

#include <allio/step_deadline.hpp>

namespace allio::detail {

template<>
struct operation_impl<io_uring_multiplexer, _event_handle, _event_handle::wait_t>
{
	/// @brief Storage for the output of the read from the eventfd.
	uint64_t event_value;

	step_deadline absolute_deadline;

	/// @brief Timeout passed by reference in a linked timeout SQE.
	io_uring_multiplexer::timeout timeout;

	/// @brief Context for the poll operation in auto reset mode.
	io_uring_multiplexer::io_data poll_data;

	static vsm::result<io_result> submit(M& m, H const& h, C const& c, S& s);
	static vsm::result<io_result> notify(M& m, H const& h, C const& c, S& s, io_status status);
	static void cancel(M& m, H const& h, C const& c, S& s);
};

} // namespace allio::detail
