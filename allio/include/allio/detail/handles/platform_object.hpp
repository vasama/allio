#pragma once

#include <allio/detail/object.hpp>
#include <allio/platform.hpp>

namespace allio::detail {

struct inheritable_t
{
	bool inheritable = false;

	friend void tag_invoke(set_argument_t, inheritable_t& a, explicit_parameter<inheritable_t>)
	{
		a.inheritable = true;
	}
};
inline constexpr explicit_parameter<inheritable_t> inheritable = {};

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

	static vsm::result<void> close(
		native_type& h,
		io_parameters_t<object_t, close_t> const& args);

	template<typename Handle>
	struct abstract_interface : base_type::abstract_interface<Handle>
	{
		[[nodiscard]] native_platform_handle platform_handle() const
		{
			return static_cast<Handle const&>(*this).native().platform_handle;
		}
	};
};

} // namespace allio::detail
