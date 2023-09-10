#pragma once

#include <allio/handles/event_handle.hpp>
#include <allio/win32/detail/iocp/multiplexer.hpp>

namespace allio::detail {

template<>
struct handle_impl<iocp_multiplexer, _event_handle>
{
	vsm::result<void> attach(iocp_multiplexer& m, _event_handle const& h);
	void detach(iocp_multiplexer& m, _event_handle const& h, error_handler* e);
};

template<>
struct operation_impl<iocp_multiplexer, _event_handle, event_handle::wait_t>
{
	using context = handle_impl<iocp_multiplexer, _event_handle>;

	win32::unique_wait_packet wait_packet;
	iocp_multiplexer::wait_slot wait_slot;

	vsm::result<void> submit(iocp_multiplexer& m, _event_handle& h, context& c);
	void cancel(iocp_multiplexer& m, _event_handle& h, context& c, error_handler* e);
};

} // namespace allio::detail
