#pragma once

#include <allio/detail/handles/process.hpp>
#include <allio/win32/handles/platform_object.hpp>

namespace allio::detail {

struct process_handle_t::impl_type : base_type::impl_type
{
	allio_handle_implementation_flags
	(
		/// @brief This handle is a pseudo handle and does not need to be closed.
		pseudo_handle,
	);
};

} // namespace allio::detail
