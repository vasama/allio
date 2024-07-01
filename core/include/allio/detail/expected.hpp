#pragma once

#include <vsm/result.hpp>
#include <vsm/utility.hpp>

namespace allio::detail {

struct expected_error_traits
{
	auto unwrap_result(auto&& result)
	{
		return vsm_forward(result);
	}
};

} // namespace allio::detail
