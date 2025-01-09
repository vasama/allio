#pragma once

//TODO: Split into core and allio aspects, with implementation in core and default API in allio.

#include <allio/handles/directory.hpp>

namespace allio::paths {

[[nodiscard]] platform_path_view temporary_directory_path();
[[nodiscard]] blocking::directory_handle const& temporary_directory();

} // namespace allio::paths
