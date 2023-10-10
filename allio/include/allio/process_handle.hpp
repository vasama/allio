#pragma once

#include <allio/default_multiplexer.hpp>
#include <allio/handles/process_handle.hpp>

namespace allio {

allio_detail_export
using async_process_handle = basic_process_handle<default_multiplexer>;

} // namespace allio
