#pragma once

#include <allio/detail/handles/platform_handle.hpp>
#include <allio/win32/detail/iocp/multiplexer.hpp>

namespace allio::detail {

struct wait_operation_state
{
	using M = iocp_multiplexer;
	using H = platform_handle;
	using S = wait_operation_state;

	win32::unique_wait_packet wait_packet;
	iocp_multiplexer::wait_slot wait_slot;

	static io_result submit(M& m, H const& h, S& s);
	static io_result notify(M& m, H const& h, S& s, io_status status);
	static void cancel(M& m, H const& h, S& s);
};

} // namespace allio::detail
