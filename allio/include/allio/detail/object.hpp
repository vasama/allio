#pragma once

#include <allio/detail/handle_flags.hpp>
#include <allio/detail/io.hpp>
#include <allio/detail/type_list.hpp>

#include <vsm/flags.hpp>

namespace allio::detail {

enum class io_flags : uint8_t
{
	create_inheritable                  = 1 << 0,
	create_multiplexable                = 1 << 1,
	async_result_stream                 = 1 << 2,
};
vsm_flag_enum(io_flags);


struct inheritable_t
{
	bool inheritable;
};

struct multiplexable_t
{
	bool multiplexable;
};

struct io_flags_t
{
	io_flags flags = {};

	friend void tag_invoke(set_argument_t, io_flags_t& args, inheritable_t const value)
	{
		if (value.inheritable)
		{
			args.flags |= io_flags::create_inheritable;
		}
	}

	friend void tag_invoke(set_argument_t, io_flags_t& args, multiplexable_t const value)
	{
		if (value.multiplexable)
		{
			args.flags |= io_flags::create_multiplexable;
		}
	}
};


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

	using params_type = no_parameters_t;
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


	using operations = type_list<close_t>;


	template<typename Handle>
	struct abstract_interface {};

	template<typename Handle>
	struct concrete_interface {};
};

} // namespace allio::detail
