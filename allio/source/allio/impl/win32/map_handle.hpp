#pragma once

#include <allio/map_handle.hpp>

namespace allio {

struct detail::map_handle_base::implementation : base_type::implementation
{
	allio_handle_implementation_flags
	(
		backing_section,
	);
};

} // namespace allio
