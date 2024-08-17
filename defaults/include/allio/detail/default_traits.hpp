#pragma once

#include <allio/detail/exceptions.hpp>
#include <allio/detail/facade.hpp>
#include <allio/detail/io_sender.hpp>

namespace allio::detail {

//TODO: Try to separate this out into different headers to avoid execution dependency for blocking.

class default_traits
{
public:
	static constexpr bool has_transform_result = true;

	template<typename T>
	static T transform_result(vsm::result<T>&& r)
	{
		return throw_on_error(vsm_move(r));
	}


	template<object Object, optional_multiplexer_handle_for<Object> MultiplexerHandle = void>
	using handle = basic_facade<basic_handle<Object, MultiplexerHandle>, default_traits>;

	template<observer Operation, detached_handle Handle>
	[[nodiscard]] static io_result_t<basic_facade<Handle, default_traits>, Operation> observe(
		basic_facade<Handle, default_traits> const& h,
		io_parameters_t<typename Handle::object_type, Operation> const& a)
	{
		using result_type = io_result_t<basic_facade<Handle, default_traits>, Operation>;

		auto r = blocking_io<Operation>(h, a);
		using value_type = typename decltype(r)::value_type;

		if (!r)
		{
			throw_error(r.error());
		}

		if constexpr (std::is_same_v<value_type, result_type>)
		{
			if constexpr (!std::is_void_v<result_type>)
			{
				return vsm_move(*r);
			}
		}
		else
		{
			return throw_on_error(rebind_handle<result_type>(vsm_move(*r)));
		}
	}

	template<observer Operation, attached_handle Handle>
	[[nodiscard]] static ex::sender auto observe(
		basic_facade<Handle, default_traits> const& h,
		io_parameters_t<typename Handle::object_type, Operation> const& a)
	{
		return io_sender<basic_facade<Handle, default_traits>, Operation>(h, a);
	}
};

class default_blocking_traits : public default_traits
{
public:
	template<object Object, producer Operation>
	static handle<Object> produce(io_parameters_t<Object, Operation> const& a)
	{
		handle<Object> h;
		throw_on_error(blocking_io<Operation>(h, a));
		return h;
	}
};

class default_sender_traits : public default_traits
{
	template<object Object>
	struct handle_template
	{
		template<multiplexer_handle_for<Object> MultiplexerHandle>
		using type = handle<Object, MultiplexerHandle>;
	};

public:
	template<object Object, producer Operation>
	static ex::sender auto produce(io_parameters_t<Object, Operation> const& a)
	{
		return io_handle_sender<Object, Operation, typename handle_template<Object>::type>(a);
	}
};

} // namespace allio::detail
