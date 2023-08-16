#pragma once

#include <allio/handles/event_handle.hpp>
#include <allio/win32/iocp/multiplexer.hpp>

#include <allio/win32/wait_packet.hpp>

namespace allio {

extern template struct async_operation_facade<win32::iocp_multiplexer, event_handle::base_type>;

template<>
struct async_operation_data<win32::iocp_multiplexer, event_handle::base_type, event_handle::wait_t>
{
	win32::unique_wait_packet wait_packet;
};
extern template struct async_operation_facade<win32::iocp_multiplexer, event_handle::base_type, event_handle::wait_t>;

} // namespace allio
