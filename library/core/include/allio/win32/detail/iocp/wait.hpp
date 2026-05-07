#pragma once

#include <allio/detail/handles/platform_object.hpp>
#include <allio/win32/detail/iocp/multiplexer.hpp>

namespace allio::detail {

struct iocp_wait_state
{
	using M = iocp_multiplexer;
	using H = native_handle<platform_object_t>;
	using S = M::operation_type;

	unique_wait_packet wait_packet;

	io_result<void> submit(M& m, H const& h, io_handler<M>& handler);
	io_result<void> notify(M& m, H const& h, io_handler<M>& handler, M::io_status_type status);
	void cancel(M& m, H const& h, io_handler<M> const& handler);
};

} // namespace allio::detail
