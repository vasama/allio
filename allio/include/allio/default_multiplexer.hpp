#pragma once

#include <allio/detail/api.hpp>

#include <vsm/platform.h>
#include <vsm/preprocessor.h>

#include vsm_pp_include(allio/vsm_os/default_multiplexer.hpp)

#define allio_detail_default_handle_include(handle) \
	vsm_pp_include(allio/vsm_os/detail/allio_detail_default_multiplexer/handle.hpp)

#if 0
#include <memory>

namespace allio {

using unique_multiplexer_ptr = std::unique_ptr<multiplexer>;

struct default_multiplexer_options
{
};

allio_detail_api vsm::result<unique_multiplexer_ptr> create_default_multiplexer();
allio_detail_api vsm::result<unique_multiplexer_ptr> create_default_multiplexer(default_multiplexer_options const& options);

} // namespace allio
#endif
