#pragma once

#include <allio/handles/opaque_handle.hpp>
#include <allio/win32/iocp/multiplexer.hpp>

#include <allio/win32/wait_packet_handle.hpp>

namespace allio {

template<>
struct async_handle_traits<win32::iocp_multiplexer, opaque_handle::base_type>
{
	struct context_type
	{
		win32::unique_wait_packet_handle wait_packet_handle;
	};
};

template<>
struct async_operation_traits<win32::iocp_multiplexer, opaque_handle::base_type, opaque_handle::poll_tag>
{
	struct storage_type
	{
		win32::iocp_multiplexer::wait_slot wait_slot;
	};
};

} // namespace allio
