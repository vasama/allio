#pragma once

#include <allio/detail/handles/platform_object.hpp>

#include <allio/detail/unique_handle.hpp>

namespace allio::detail {

struct handle_with_flags
{
	unique_handle handle;
	handle_flags flags;
};

} // namespace allio::detail
