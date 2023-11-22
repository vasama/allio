#pragma once

#include <allio/detail/handles/platform_object.hpp>
#include <allio/detail/io_sender.hpp>

namespace allio::detail {

struct timer_t : platform_object_t
{
	using base_type = platform_object_t;

	struct create_t
	{
		using operation_concept = producer_t;
		
		using required_params_type = no_parameters_t;
		using optional_params_type = no_parameters_t;
		
		using runtime_tag = bounded_runtime_t;
	};

	using operations = type_list_append
	<
		base_type::operations
		, create_t
	>;

	vsm::result<void> blocking_io(
		create_t,
		native_type& h,
		io_parameters_t<create_t> const& args);
};

} // namespace allio::detail
