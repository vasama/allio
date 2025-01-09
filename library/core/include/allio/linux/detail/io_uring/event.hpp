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
struct async_operation<io_uring_multiplexer, event_t, event_io::wait_t>
	: io_uring_multiplexer::operation_type
{
	using M = io_uring_multiplexer;
	using H = event_t::native_type const;
	using C = async_connector_t<M, event_t> const;
	using S = async_operation_t<M, event_t, event_io::wait_t>;
	using A = io_parameters_t<event_t, event_io::wait_t>;

	//TODO: Pass handler to notify instead of storing it.
	io_handler<M>* handler;

	step_deadline absolute_deadline;

	/// @brief Timeout passed by reference in a linked timeout SQE.
	M::timeout timeout;

	/// @brief Context for the poll operation.
	M::io_slot poll_slot;

	static io_result<void> submit(M& m, H& h, C& c, S& s, A const& a, io_handler<M>& handler);
	static io_result<void> notify(M& m, H& h, C& c, S& s, A const& a, M::io_status_type status);
	static void cancel(M& m, H const& h, C const& c, S& s);
};

} // namespace allio::detail
