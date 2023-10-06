#pragma once

#include <allio/detail/handles/platform_handle.hpp>
#include <allio/win32/detail/iocp/multiplexer.hpp>

namespace allio::detail {

struct wait_operation_state
{
	using M = iocp_multiplexer;
	using H = platform_handle;
	using S = iocp_multiplexer::operation_type;

	win32::unique_wait_packet wait_packet;
	iocp_multiplexer::wait_slot wait_slot;

	io_result submit(M& m, H const& h, S& s);
	io_result notify(M& m, H const& h, S& s, io_status status);
	void cancel(M& m, H const& h, S& s);
};

} // namespace allio::detail
