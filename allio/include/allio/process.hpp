#pragma once

#include <allio/default_multiplexer.hpp>
#include <allio/handles/process.hpp>

namespace allio::async {

using process_handle = basic_process_handle<default_multiplexer_handle>;

} // namespace allio::async
