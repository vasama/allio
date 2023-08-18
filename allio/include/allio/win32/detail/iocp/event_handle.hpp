#pragma once

#include <allio/handles/event_handle.hpp>
#include <allio/win32/detail/iocp/multiplexer.hpp>

namespace allio::detail {

extern template struct async_handle_facade<iocp_multiplexer, event_handle::base_type>;

template<>
struct async_operation_data<iocp_multiplexer, event_handle::base_type, event_handle::wait_t>
{
	win32::unique_wait_packet wait_packet;
	iocp_multiplexer::wait_slot wait_slot;
};
extern template struct async_operation_facade<iocp_multiplexer, event_handle::base_type, event_handle::wait_t>;

} // namespace allio::detail
