#pragma once

#include <allio/detail/handles/event.hpp>
#include <allio/linux/detail/io_uring/multiplexer.hpp>

#include <allio/step_deadline.hpp>

namespace allio::detail {

template<>
struct async_connector<io_uring_multiplexer, event_t>
	: io_uring_multiplexer::connector_type
{
};

template<>
struct async_operation<io_uring_multiplexer, event_t, event_t::wait_t>
	: io_uring_multiplexer::operation_type
{
	using M = io_uring_multiplexer;
	using H = native_handle<event_t> const;
	using C = async_connector_t<M, event_t> const;
	using S = async_operation_t<M, event_t, event_t::wait_t>;
	using A = io_parameters_t<event_t, event_t::wait_t>;

	step_deadline absolute_deadline;

	union
	{
		/// @brief Timeout passed by reference in a linked timeout SQE.
		M::timeout timeout;

		/// @brief Result of the poll operation in the case that
		///        1. the event is in auto reset mode (needs read),
		///        2. cqe skipping on success is not available, and
		///        3. the poll failed, but the read is still pending.
		int previous_error;
	};

	static io_result<void> submit(M& m, H& h, C& c, S& s, A const& a, io_handler<M>& handler);
	static io_result<void> notify(M& m, H& h, C& c, S& s, A const& a, io_handler<M>& handler, M::io_status_type status);
	static void cancel(M& m, H const& h, C const& c, S& s);
};

} // namespace allio::detail
