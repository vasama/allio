#pragma once

#include <allio/detail/handles/file.hpp>

namespace allio::detail {

#if vsm_os_linux
struct file_t::impl_type : base_type::impl_type
{
	allio_handle_implementation_flags
	(
		/// @brief The file was opened with O_SYNC or O_DSYNC.
		sync_data,

		/// @brief The file was opened with O_SYNC.
		sync_attr,
	);
};
#endif

} // namespace allio::detail
