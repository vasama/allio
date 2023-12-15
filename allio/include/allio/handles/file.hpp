#pragma once

#include <allio/detail/handles/file.hpp>
#include <allio/handles/fs_object.hpp>

namespace allio {

using detail::file_t;
using abstract_file_handle = detail::abstract_handle<file_t>;

} // namespace allio
