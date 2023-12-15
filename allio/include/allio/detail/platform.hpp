#pragma once

#include <vsm/platform.h>

#if vsm_os_posix
	#include <allio/posix/detail/platform.hpp>
#else
	#include vsm_pp_include(allio/vsm_os/detail/platform.hpp)
#endif

namespace allio::detail {

enum class native_platform_handle : platform_handle_uint_type
{
	null = 0
};

inline native_platform_handle wrap_handle(platform_handle_type const handle)
{
	return (native_platform_handle)((platform_handle_uint_type)handle + platform_handle_offset);
}

inline platform_handle_type unwrap_handle(native_platform_handle const handle)
{
	return (platform_handle_type)((platform_handle_uint_type)handle - platform_handle_offset);
}

void close_platform_handle(platform_handle_type handle) noexcept;

} // namespace allio::detail
