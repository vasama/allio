#pragma once

namespace allio::detail {

using platform_handle_type = int;
inline constexpr int null_platform_handle = -1;

using platform_handle_uint_type = unsigned;
inline constexpr unsigned platform_handle_offset = 1;

} // namespace allio::detail
