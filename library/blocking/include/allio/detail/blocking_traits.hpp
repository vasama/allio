#pragma once

#include <allio/detail/exceptions.hpp>
#include <allio/detail/facade.hpp>

namespace allio::detail {

class blocking_traits
{
public:
	using detached_traits = blocking_traits;

	static constexpr bool has_transform_result = true;

	template<typename T>
	[[nodiscard]] static T transform_result(vsm::result<T>&& r)
	{
		return throw_on_error(vsm_move(r));
	}

	template<object Object, optional_multiplexer_handle_for<Object> MultiplexerHandle = void>
	using handle = basic_facade<basic_handle<Object, MultiplexerHandle>, blocking_traits>;

	template<observer Operation, detached_handle Handle, std::derived_from<blocking_traits> Traits>
	[[nodiscard]] static io_result_t<basic_facade<Handle, Traits>, Operation> observe(
		basic_facade<Handle, Traits> const& h,
		io_parameters_t<typename Handle::object_type, Operation> const& a)
	{
		using result_type = io_result_t<basic_facade<Handle, Traits>, Operation>;

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

	template<object Object, producer Operation>
	static handle<Object> produce(io_parameters_t<Object, Operation> const& a)
	{
		handle<Object> h;
		throw_on_error(blocking_io<Operation>(h, a));
		return h;
	}

	template<typename PreviousResult, typename Function>
	static auto transform(PreviousResult&& previous_result, Function&& function)
	{
		return vsm_forward(function)(vsm_forward(previous_result));
	}

	template<typename PreviousResult, typename Function>
	static auto and_then(PreviousResult&& previous_result, Function&& function)
	{
		return throw_on_error(vsm_forward(function)(vsm_forward(previous_result)));
	}
};

} // namespace allio::detail
