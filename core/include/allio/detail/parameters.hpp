#pragma once

#include <vsm/concepts.hpp>
#include <vsm/standard.hpp>
#include <vsm/tag_invoke.hpp>
#include <vsm/utility.hpp>

#include <utility>

namespace allio::detail {

template<typename... Parameters>
[[deprecated]] struct parameters_t : Parameters... {};

using no_parameters_t = parameters_t<>;


#if 0
struct set_argument_t
{
	template<typename Parameter>
	friend constexpr void tag_invoke(set_argument_t, Parameter& args, Parameter const& value)
	{
		args = value;
	}

	template<typename Parameters, typename Argument>
	vsm_static_operator constexpr void
	operator()(Parameters& arguments, Argument&& argument) vsm_static_operator_const
		requires vsm::tag_invocable<set_argument_t, Parameters&, Argument&&>
	{
		vsm::tag_invoke(set_argument_t(), arguments, vsm_forward(argument));
	}
};
#else
struct set_argument_t
{
	template<typename Parameters, typename Argument>
	vsm_static_operator constexpr void operator()(
		Parameters& arguments,
		Argument&& new_argument) vsm_static_operator_const
	{
		if constexpr (vsm::any_cvref_of<Argument, Parameters>)
		{
			arguments = vsm_forward(new_argument);
		}
		else
		{
			arguments.set_argument(vsm_forward(new_argument));
		}
	}
};
#endif
inline constexpr set_argument_t set_argument = {};


template<typename P, typename T>
struct explicit_argument
{
	using value_type = T;

	T value;
};

template<typename P>
struct explicit_parameter
{
	static_assert(std::is_object_v<typename P::value_type>);

	template<std::convertible_to<typename P::value_type> Value>
	vsm_static_operator P operator()(Value&& value) vsm_static_operator_const
	{
		return { explicit_argument<P, typename P::value_type>{ vsm_forward(value) } };
	}
};

template<typename P>
struct explicit_reference_parameter
{
	static_assert(std::is_pointer_v<typename P::value_type>);

	vsm_static_operator P operator()(std::remove_pointer_t<typename P::value_type>& value) vsm_static_operator_const
	{
		return { explicit_argument<P, typename P::value_type>{ &value } };
	}
};


template<typename P>
P make_args(auto&&... args)
{
	P arguments = {};
	(set_argument(arguments, vsm_forward(args)), ...);
	return arguments;
}

} // namespace allio::detail
