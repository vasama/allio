#pragma once

#include <allio/detail/object.hpp>
#include <allio/detail/platform.hpp>

namespace allio::detail {

struct platform_object_t : object_t
{
	using base_type = object_t;

	struct impl_type;

	struct native_type : base_type::native_type
	{
		native_platform_handle platform_handle;
	};

	static vsm::result<void> close(
		native_type& h,
		io_parameters_t<object_t, close_t> const& args);
};

} // namespace allio::detail
