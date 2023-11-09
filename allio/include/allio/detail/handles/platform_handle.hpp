#pragma once

#include <allio/detail/handle.hpp>
#include <allio/platform.hpp>

namespace allio::detail {

struct platform_object_t : object_t
{
	using base_type = object_t;

	struct impl_type;

	struct native_type : base_type::native_type
	{
		native_platform_handle platform_handle;
	};

	static void zero_native_handle(native_type& h)
	{
		base_type::zero_native_handle(h);
		h.platform_handle = native_platform_handle::null;
	}

	template<typename H>
	struct abstract_interface : base_type::abstract_interface<H>
	{
		[[nodiscard]] native_platform_handle platform_handle() const
		{
			return static_cast<H const&>(*this).native().platform_handle;
		}
	};

	static vsm::result<void> blocking_io(
		close_t,
		native_type& h,
		io_parameters_t<close_t> const& args);
};

} // namespace allio::detail
