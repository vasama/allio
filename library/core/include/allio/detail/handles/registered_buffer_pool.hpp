#pragma once

#include <allio/detail/buffer_registrar.hpp>

namespace allio::detail {

struct create_buffer_pool_t
{
	using operation_concept = producer_t;

	struct params_type
	{
		buffer_registrar* registrar;
		std::byte* storage;
		size_t buffer_size;
		size_t buffer_count;
	};

	using result_type = void;
};

struct buffer_pool_t : object_t
{
	using base_type = object_t;

	using create_buffer_pool_t = detail::create_buffer_pool_t;

	using operations = type_list_append
	<
		base_type::operations
		, create_buffer_pool_t
	>;
};

template<>
struct native_handle<buffer_pool_t> : native_handle<buffer_pool_t::base_type>
{
	buffer_registrar* registrar;
	void* opaque_pointer;
};


class buffer_handle
{
	

public:
};


} // namespace allio::detail
