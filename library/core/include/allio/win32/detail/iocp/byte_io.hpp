#pragma once

#include <allio/byte_io.hpp>
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

	size_t transferred;
	uint32_t buffer_index;
	uint32_t buffer_visit;

	iocp_multiplexer::io_status_block io_status_block;

	template<typename Arguments>
	io_result<size_t> submit(H const& h, Arguments const& a, io_handler<M>& handler);

	template<typename Arguments>
	io_result<size_t> notify(H const& h, Arguments const& a, M::io_status_type status);

	void cancel(M& m, H const& h);
};

} // namespace allio::detail
