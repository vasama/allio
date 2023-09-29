#pragma once

#include <allio/default_multiplexer.hpp>
#include <allio/handles/opaque_handle.hpp>

#include allio_detail_default_handle_include(opaque_handle)

namespace allio {

allio_detail_export
using async_opaque_handle = basic_opaque_handle<default_multiplexer_ptr>;

} // namespace allio
