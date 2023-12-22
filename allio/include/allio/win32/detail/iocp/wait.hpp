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
	iocp_multiplexer::wait_slot wait_slot;

	io_result<void> submit(M& m, H const& h, S& s, io_handler<M>& handler);
	io_result<void> notify(M& m, H const& h, S& s, M::io_status_type status);
	void cancel(M& m, H const& h, S& s);
};

} // namespace allio::detail
