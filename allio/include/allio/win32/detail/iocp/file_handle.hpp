#pragma once

#include <allio/handles/file_handle.hpp>
#include <allio/win32/iocp/byte_io.hpp>
#include <allio/win32/iocp/multiplexer.hpp>

namespace allio {

template<>
struct async_handle_traits<win32::iocp_multiplexer, file_handle::base_type>
{
	struct context_type
	{
	};
};

template<>
struct async_operation_traits<win32::iocp_multiplexer, file_handle::base_type, scatter_read_at_tag>
{
	struct storage_type
	{
	};
};

template<>
struct async_operation_traits<win32::iocp_multiplexer, file_handle::base_type, gather_write_at_tag>
{
	struct storage_type
	{
	};
};

} // namespace allio
