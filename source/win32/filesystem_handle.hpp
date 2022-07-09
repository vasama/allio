#pragma once

#include <allio/filesystem_handle.hpp>

#include "platform_handle.hpp"

namespace allio::win32 {

result<unique_handle> create_file(filesystem_handle const* const base, path_view const path, file_parameters const& args);

} // namespace allio::win32
