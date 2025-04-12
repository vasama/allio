#pragma once

//TODO: Move this header under handles/ ?

#include <allio/detail/handle_flags.hpp>
#include <allio/detail/io.hpp>
#include <allio/detail/type_list.hpp>

#include <vsm/flags.hpp>

namespace allio::detail {

class serialization_context;


enum class io_flags : uint8_t
{
	none                                = 0,
	create_inheritable                  = 1 << 0,
	create_synchronous                  = 1 << 1,
	create_non_blocking                 = 1 << 2,
	create_registered_io                = 1 << 3,
	multishot                           = 1 << 4,
	greedy_byte_io                      = 1 << 5,
};
vsm_flag_enum(io_flags);


struct inheritable_t : explicit_argument<inheritable_t, bool> {};
inline constexpr explicit_parameter<inheritable_t> inheritable = {};

struct synchronous_t : explicit_argument<synchronous_t, bool> {};
inline constexpr explicit_parameter<synchronous_t> synchronous = {};

struct non_blocking_t : explicit_argument<non_blocking_t, bool> {};
inline constexpr explicit_parameter<non_blocking_t> non_blocking = {};

struct set_io_flags_t : explicit_argument<set_io_flags_t, io_flags> {};

struct io_flags_t
{
	io_flags flags = {};

	void set_argument(explicit_parameter<inheritable_t>)
	{
		flags |= io_flags::create_inheritable;
	}

	void set_argument(inheritable_t const value)
	{
		if (value.value)
		{
			flags |= io_flags::create_inheritable;
		}
	}

	void set_argument(explicit_parameter<synchronous_t>)
	{
		flags |= io_flags::create_synchronous;
	}

	void set_argument(synchronous_t const value)
	{
		if (value.value)
		{
			flags |= io_flags::create_synchronous;
		}
	}

	void set_argument(explicit_parameter<non_blocking_t>)
	{
		flags |= io_flags::create_non_blocking;
	}

	void set_argument(non_blocking_t const value)
	{
		if (value.value)
		{
			flags |= io_flags::create_non_blocking;
		}
	}

	void set_argument(set_io_flags_t const value)
	{
		flags |= value.value;
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

template<object Object>
struct native_handle : native_handle<typename Object::base_type>
{
};


struct close_t
{
	using operation_concept = consumer_t;

	using params_type = no_parameters_t;
	using result_type = void;

	template<object Object>
	static vsm::result<void> blocking_io(
		native_handle<Object>& h,
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

	using operations = type_list<close_t>;

	static vsm::result<void> serializer_visit(
		native_handle<object_t>& h,
		serialization_context& serializer);

	template<typename Handle, typename Traits>
	struct facade {};
};

template<>
struct native_handle<object_t>
{
	handle_flags flags;
};

} // namespace allio::detail
