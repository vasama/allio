#pragma once

#include <allio/detail/handles/pipe.hpp>

namespace allio::detail {

#if vsm_os_linux
struct platform_object_t::impl_type : base_type::impl_type
{
	allio_handle_implementation_flags
	(
		/// @brief Raising SIGPIPE as a consequence of operations on this handle is permitted.
		may_signal,
	);
};
#endif

} // namespace allio::detail
