#pragma once

#include <allio/detail/handles/file.hpp>

namespace allio::detail {

#if vsm_os_win32
struct file_t::impl_type : base_type::impl_type
{
	allio_handle_implementation_flags
	(
		/// @brief This handle provides data and attribute durability guarantees.
		flush,
	);
};
#endif

} // namespace allio::detail
