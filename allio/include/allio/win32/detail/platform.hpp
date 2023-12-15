#pragma once

#include <cstdint>

namespace allio::detail {

using platform_handle_type = void*;
inline constexpr void* null_platform_handle = nullptr;

using platform_handle_uint_type = uintptr_t;
inline constexpr uintptr_t platform_handle_offset = 0;

} // namespace allio::detail
