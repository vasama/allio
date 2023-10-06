#pragma once

#include <allio/detail/handles/process_handle.hpp>
#include <allio/win32/handles/platform_handle.hpp>

namespace allio::detail {

struct _process_handle::impl_type : base_type::impl_type
{
	allio_handle_implementation_flags
	(
		/// @brief This handle is a pseudo handle and does not need to be closed.
		pseudo_handle,
	);
};

} // namespace allio::detail
