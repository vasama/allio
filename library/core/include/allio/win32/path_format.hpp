#pragma once

#include <allio/detail/filesystem.hpp>

namespace allio::win32 {

inline constexpr detail::path_format dos_format = detail::path_format(1);
inline constexpr detail::path_format uid_format = detail::path_format(2);
inline constexpr detail::path_format dev_format = detail::path_format(3);

} // namespace allio::win32
