#pragma once

#include <allio/detail/handles/platform_object.hpp>

namespace allio::detail {

#if vsm_os_linux
struct platform_object_t::impl_type : base_type::impl_type
{
	allio_handle_implementation_flags
	(
		/// @brief The handle was created in non-blocking mode.
		non_blocking,
	);
};
#endif

} // namespace allio::detail
