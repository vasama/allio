#pragma once

#include <allio/detail/handle_flags.hpp>
#include <allio/detail/io.hpp>
#include <allio/detail/type_list.hpp>

namespace allio::detail {

struct _object
{
	struct flags : handle_flags
	{
		flags() = delete;
		static constexpr handle_flags::uint_type allio_detail_handle_flags_flag_count = 0;
		static constexpr handle_flags::uint_type allio_detail_handle_flags_flag_limit = 16;
	};

	struct impl_type
	{
		struct flags : handle_flags
		{
			flags() = delete;
			static constexpr handle_flags::uint_type allio_detail_handle_flags_flag_count = 16;
			static constexpr handle_flags::uint_type allio_detail_handle_flags_flag_limit = 32;
		};
	};

	_object() = delete;
};

struct close_t
{
	using operation_concept = consumer_t;

	using required_params_type = no_parameters_t;
	using optional_params_type = no_parameters_t;
	using result_type = void;

	template<object Object>
	friend vsm::result<void> tag_invoke(
		blocking_io_t<Object, close_t>,
		typename Object::native_type& h,
		io_parameters_t<Object, close_t> const& a)
		requires requires { Object::close(h, a); }
	{
		return Object::close(h, a);
	}
};

struct object_t : _object
{
	using base_type = _object;

	allio_handle_flags
	(
		not_null,
	);


	struct native_type
	{
		handle_flags flags;
	};

	static bool test_native_handle(native_type const& h)
	{
		return h.flags[flags::not_null];
	}

	static void zero_native_handle(native_type& h)
	{
		h.flags = {};
	}


	using operations = type_list<close_t>;


	template<typename Handle>
	struct abstract_interface {};

	template<typename Handle, optional_multiplexer_handle_for<object_t> MultiplexerHandle>
	struct concrete_interface {};
};

} // namespace allio::detail
