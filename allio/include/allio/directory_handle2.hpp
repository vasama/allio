#pragma once

#include <allio/default_multiplexer.hpp>
#include <allio/handles/directory_handle2.hpp>

#include allio_detail_default_handle_include(directory_handle)

namespace allio {

allio_detail_export
using async_directory_handle = basic_directory_handle<default_multiplexer>;

} // namespace allio
