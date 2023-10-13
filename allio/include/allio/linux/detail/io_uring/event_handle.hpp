#pragma once

#include <allio/detail/handles/event_handle.hpp>
#include <allio/linux/detail/io_uring/multiplexer.hpp>

#include <allio/step_deadline.hpp>

namespace allio::detail {

template<>
struct operation_impl<io_uring_multiplexer, event_handle_t, event_handle_t::wait_t>
{
	using M = io_uring_multiplexer;
	using H = event_handle_t;
	using O = event_handle_t::wait_t;
	using C = connector_t<M, H>;
	using S = operation_t<M, H, O>;

	step_deadline absolute_deadline;

	/// @brief Timeout passed by reference in a linked timeout SQE.
	io_uring_multiplexer::timeout timeout;

	/// @brief Context for the poll operation.
	io_uring_multiplexer::io_slot poll_slot;

	static io_result2 submit(M& m, H const& h, C const& c, S& s);
	static io_result2 notify(M& m, H const& h, C const& c, S& s, io_status status);
	static void cancel(M& m, H const& h, C const& c, S& s);
};

} // namespace allio::detail
