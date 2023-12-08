#pragma once

#include <vsm/concepts.hpp>
#include <vsm/tag_invoke.hpp>
#include <vsm/utility.hpp>

#include <utility>

namespace allio::detail {

struct set_argument_t
{
	template<typename Parameter>
	friend constexpr void tag_invoke(set_argument_t, Parameter& args, Parameter const& value)
	{
		args = value;
	}

	template<typename Parameters, typename Argument>
	constexpr void vsm_static_operator_invoke(Parameters& arguments, Argument&& argument)
		requires vsm::tag_invocable<set_argument_t, Parameters&, Argument&&>
	{
		vsm::tag_invoke(set_argument_t(), arguments, vsm_forward(argument));
	}
};
inline constexpr set_argument_t set_argument = {};


template<typename P>
auto&& parameter_value(P&& args)
{
	auto&& [value] = vsm_forward(args);
#if __cpp_lib_forward_like
	return std::forward_like<P>(value);
#else
	return static_cast<vsm::copy_cvref_t<P&&, decltype(value)>>(value);
#endif
}

template<typename P>
using parameter_value_t = std::remove_cvref_t<decltype(parameter_value(std::declval<P>()))>;


template<typename P, typename T = void>
class explicit_parameter
{
	using value_type = vsm::select_t<std::is_void_v<T>, parameter_value_t<P>, T>;

	struct argument
	{
		value_type value;

		friend void tag_invoke(
			set_argument_t,
			P& arguments,
			vsm::any_cvref_of<argument> auto&& argument)
		{
			parameter_value(arguments) = vsm_forward(argument).value;
		}
	};

public:
	constexpr argument vsm_static_operator_invoke(std::convertible_to<value_type> auto&& value)
	{
		return { { vsm_forward(value) } };
	}
};

template<typename P, typename T = void>
class explicit_ref_parameter
{
	using pointer_type = vsm::select_t<std::is_void_v<T>, parameter_value_t<P>, T*>;
	static_assert(std::is_pointer_v<pointer_type>);
	using value_type = std::remove_pointer_t<pointer_type>;

	struct argument_t
	{
		pointer_type value;

		friend void tag_invoke(
			set_argument_t,
			P& arguments,
			vsm::any_cvref_of<argument_t> auto&& argument)
		{
			parameter_value(arguments) = vsm_forward(argument).value;
		}
	};

public:
	constexpr argument_t vsm_static_operator_invoke(value_type& value)
	{
		return { { &value } };
	}
};


template<typename... Parameters>
struct parameters_t : Parameters... {};

struct no_parameters_t {};


template<typename P>
P make_args(auto&&... args)
{
	P arguments;
	(set_argument(arguments, vsm_forward(args)), ...);
	return arguments;
}

} // namespace allio::detail
