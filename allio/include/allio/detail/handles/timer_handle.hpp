#pragma once

#include <allio/detail/handles/platform_handle.hpp>
#include <allio/detail/io_sender.hpp>

namespace allio::detail {

struct timer_t : platform_handle_t
{
	using base_type = platform_handle_t;

	struct create_t
	{
		using is_mutation = void;
		
		using required_params_type = no_parameters_t;
		using optional_params_type = no_parameters_t;
		
		using runtime_tag = bounded_runtime_t;
	};

	using asynchronous_operations = type_list_cat<
		base_type::asynchronous_operations,
		type_list<create_t>
	>;

	vsm::result<void> blocking_io(
		create_t,
		native_type& h,
		io_parameters_t<create_t> const& args);
};

} // namespace allio::detail
