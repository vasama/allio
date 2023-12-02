#pragma once

#include <allio/handles/directory.hpp>

namespace allio::paths {

platform_path_view temporary_directory_path();
blocking::directory_handle const& temporary_directory();

} // namespace allio::paths
