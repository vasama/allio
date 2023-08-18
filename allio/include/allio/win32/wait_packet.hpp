#pragma once

#include <vsm/unique_resource.hpp>

#include <bit>

#include <cstdint>

namespace allio::win32 {

enum class wait_packet : uintptr_t
{
	null = 0
};

inline wait_packet wrap_wait_packet(detail::HANDLE const handle)
{
	return std::bit_cast<wait_packet>(handle);
}

inline detail::HANDLE unwrap_wait_packet(wait_packet const handle)
{
	return std::bit_cast<detail::HANDLE>(handle);
}


struct wait_packet_deleter
{
	void vsm_static_operator_invoke(wait_packet const wait_packet);
};

using unique_wait_packet = vsm::unique_resource<wait_packet, wait_packet_deleter, wait_packet::null>;

} // namespace allio::win32
