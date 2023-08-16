#pragma once

#include <allio/handles/event_handle2.hpp>
#include <allio/win32/iocp/multiplexer.hpp>

#include <allio/win32/wait_packet.hpp>

namespace allio {

template<>
struct async_handle_traits<win32::iocp_multiplexer, event_handle::base_type>
{
	struct context_type
	{
	};
};

template<>
struct async_operation_traits<win32::iocp_multiplexer, event_handle::base_type, event_handle::wait_tag>
{
	struct storage_type
	{
		win32::unique_wait_packet wait_packet;
		win32::iocp_multiplexer::io_status_block slot;
	};
};

} // namespace allio
