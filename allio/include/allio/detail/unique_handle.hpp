#pragma once

#include <allio/detail/platform.hpp>

#include <vsm/standard.hpp>
#include <vsm/unique_resource.hpp>

namespace allio::detail {

struct platform_handle_deleter
{
	void vsm_static_operator_invoke(platform_handle_type const handle) noexcept
	{
		close_platform_handle(handle);
	}
};

using unique_handle = vsm::unique_resource<
	platform_handle_type,
	platform_handle_deleter,
	null_platform_handle>;

} // namespace allio::detail
