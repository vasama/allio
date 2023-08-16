#pragma once

#include <allio/default_multiplexer.hpp>
#include <allio/handles/file_handle2.hpp>

#include allio_detail_default_handle_include(file_handle)

namespace allio {

allio_detail_export
using async_file_handle = basic_file_handle<default_multiplexer>;

} // namespace allio
