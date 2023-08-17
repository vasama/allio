#pragma once

#include <allio/detail/handles/platform_handle.hpp>

namespace allio::detail {

struct platform_handle::impl_type : base_type::impl_type
{
	allio_handle_implementation_flags
	(
		//TODO: Explain the purpose of each flag.
		overlapped,
		skip_completion_port_on_success,
		skip_handle_event_on_completion,
	);
};

} // namespace allio::detail
