#pragma once

#include <allio/detail/handles/standard_stream.hpp>

namespace allio::detail {

#if vsm_os_win32
struct standard_stream_t::impl_type : base_type::impl_type
{
	allio_handle_implementation_flags
	(
		/// @brief The native handle refers to a console object.
		console,
	);
};
#endif

} // namespace allio::detail
