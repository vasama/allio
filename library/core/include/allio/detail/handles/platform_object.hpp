#pragma once

#include <allio/detail/handle.hpp>
#include <allio/detail/platform.hpp>

namespace allio::detail {

struct platform_object_t : object_t
{
	using base_type = object_t;

	struct impl_type;

	static vsm::result<void> close(
		native_handle<platform_object_t>& h,
		io_parameters_t<object_t, close_t> const& args);
};

template<>
struct native_handle<platform_object_t> : native_handle<platform_object_t::base_type>
{
	native_platform_handle platform_handle;
};

template<typename Object>
concept platform_object = object<Object> && std::derived_from<Object, platform_object_t>;

} // namespace allio::detail
