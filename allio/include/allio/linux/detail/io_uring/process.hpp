#pragma once

#include <allio/detail/handles/process.hpp>
#include <allio/linux/detail/io_uring/multiplexer.hpp>

namespace allio::detail {

template<>
struct async_connector<io_uring_multiplexer, process_t>
	: io_uring_multiplexer::connector_type
{
	using M = io_uring_multiplexer;
	using H = process_t::native_type;
	using C = async_connector_t<M, process_t>;


};

template<>
struct async_operation<io_uring_multiplexer, process_t, process_io::wait_t>
	: io_uring_multiplexer::operation_type
{
	using M = io_uring_multiplexer;
	using H = process_t::native_type const;
	using C = async_connector_t<M, process_t> const;
	using S = async_operation_t<M, process_t, process_io::wait_t>;
	using A = io_parameters_t<process_t, process_io::wait_t>;

	/// @brief Timeout passed by reference in a linked timeout SQE.
	M::timeout timeout;

	static io_result<process_exit_code> submit(M& m, H& h, C& c, S& s, A const& a, io_handler<M>& handler);
	static io_result<process_exit_code> notify(M& m, H& h, C& c, S& s, A const& a, M::io_status_type status);
	static void cancel(M& m, H const& h, C const& c, S& s);
};

} // namespace allio::detail
