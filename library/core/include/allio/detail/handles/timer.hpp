#pragma once

#include <allio/detail/handles/platform_object.hpp>

namespace allio::detail {

struct timer_t : platform_object_t
{
	using base_type = platform_object_t;

	struct create_t
	{
		using operation_concept = producer_t;

		struct params_type : io_flags_t
		{
			using io_flags_t::set_argument;
		};

		using result_type = void;
		using runtime_concept = bounded_runtime_t;

		template<object Object>
		static constexpr auto& blocking_io = Object::create;
	};

	using operations = type_list_append
	<
		base_type::operations
		, create_t
	>;

	static vsm::result<void> create(
		native_handle<create_t>& h,
		io_parameters_t<timer_t, create_t> const& a);
};

} // namespace allio::detail
