#pragma once

#include <allio/detail/handles/platform_object.hpp>

namespace allio::detail {

struct platform_object_t::impl_type : base_type::impl_type
{
	allio_handle_implementation_flags
	(
		/// @brief The handle is a pseudo handle and does not need to be closed.
		pseudo_handle,
	
		/// @brief The handle is synchronous and cannot be attached to a completion port.
		/// @see FILE_SYNCHRONOUS_IO_NONALERT
		synchronous,

		/// @brief The handle does not queue completions to an attached completion port
		///        on synchronous I/O completion.
		/// @see SetFileCompletionNotificationModes
		skip_completion_port_on_success,

		/// @brief The handle does not signal its internal event object on completion of
		///        asynchronous I/O operations.
		/// @see SetFileCompletionNotificationModes
		skip_handle_event_on_completion,
	);
};

} // namespace allio::detail
