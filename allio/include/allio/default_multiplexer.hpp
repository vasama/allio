#pragma once

#include <allio/detail/api.hpp>

#include <vsm/platform.h>
#include <vsm/preprocessor.h>

#include vsm_pp_include(allio/vsm_os/default_multiplexer.hpp)

#define allio_detail_default_handle_include(handle) \
	vsm_pp_include(allio/vsm_os/detail/allio_detail_default_multiplexer/handle.hpp)

namespace allio {

using default_multiplexer_ptr = default_multiplexer*;

} // namespace allio
