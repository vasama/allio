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
	return (wait_packet)(platform_handle_uint_type)handle;
}

inline platform_handle_type unwrap_wait_packet(wait_packet const handle)
{
	return (platform_handle_type)(platform_handle_uint_type)handle;
}


struct wait_packet_deleter
{
	void vsm_static_operator_invoke(wait_packet const handle)
	{
		close_platform_handle(unwrap_wait_packet(handle));
	}
};

using unique_wait_packet = vsm::unique_resource<
	wait_packet,
	wait_packet_deleter,
	wait_packet::null>;

} // namespace allio::detail
