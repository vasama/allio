#pragma once

#include <allio/detail/unique_handle.hpp>

#include <bit>

namespace allio::detail {

enum class wait_packet : platform_handle_uint_type
{
	null = 0
};

inline wait_packet wrap_wait_packet(platform_handle_type const handle)
{
	return static_cast<wait_packet>(reinterpret_cast<platform_handle_uint_type>(handle));
}

inline platform_handle_type unwrap_wait_packet(wait_packet const handle)
{
	return reinterpret_cast<platform_handle_type>(static_cast<platform_handle_uint_type>(handle));
}


struct wait_packet_deleter
{
	vsm_static_operator void operator()(wait_packet const handle) vsm_static_operator_const
	{
		close_platform_handle(unwrap_wait_packet(handle));
	}
};

using unique_wait_packet = vsm::unique_resource<
	wait_packet,
	wait_packet_deleter,
	wait_packet::null>;

} // namespace allio::detail
