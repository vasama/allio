#pragma once

#include <allio/default_multiplexer.hpp>
#include <allio/handles/directory.hpp>

namespace allio::async {

using directory_handle = basic_directory_handle<default_multiplexer_handle>;

} // namespace allio::async
