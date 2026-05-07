#pragma once

#include <allio/byte_io.hpp>
#include <allio/detail/byte_io_state.hpp>
#include <allio/detail/handles/platform_object.hpp>
#include <allio/step_deadline.hpp>
#include <allio/win32/detail/iocp/multiplexer.hpp>

namespace allio::detail {

struct iocp_byte_io_state
{
	using M = iocp_multiplexer;
	using H = native_handle<platform_object_t>;
	using S = M::operation_type;

	step_deadline absolute_deadline;

	byte_io_state io_state;
	uint32_t buffer_index;
	uint32_t buffer_visit;

	iocp_multiplexer::io_status_block io_status_block;

	template<typename Operation>
	io_result<typename Operation::result_type> submit(
		H const& h,
		typename Operation::params_type const& a,
		io_handler<M>& handler);

	template<typename Operation>
	io_result<typename Operation::result_type> notify(
		H const& h,
		typename Operation::params_type const& a,
		io_handler<M>& handler,
		M::io_status_type status);

	void cancel(M& m, H const& h);
};

} // namespace allio::detail
