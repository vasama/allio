#pragma once

#include <allio/detail/api.hpp>

#include <vsm/platform.h>
#include <vsm/preprocessor.h>

#include vsm_pp_include(allio/vsm_os/default_multiplexer.hpp)

namespace allio {

using default_multiplexer_ptr = default_multiplexer*;

} // namespace allio
