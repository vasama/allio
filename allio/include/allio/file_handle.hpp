#pragma once

#include <allio/default_multiplexer.hpp>
#include <allio/handles/file_handle.hpp>

namespace allio::async {

using file_handle = basic_file_handle<default_multiplexer_handle>;

} // namespace allio::async
