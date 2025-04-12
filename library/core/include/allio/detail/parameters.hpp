#pragma once

#include <vsm/concepts.hpp>
#include <vsm/standard.hpp>
#include <vsm/tag_invoke.hpp>
#include <vsm/utility.hpp>

#include <utility>

namespace allio::detail {

struct no_parameters_t {};


template<typename P, P PointerToMember, typename U>
class assign_member_t;

template<typename T, typename C, T(C::* PointerToMember), typename U>
	requires std::is_assignable_v<T, U>
class assign_member_t<T(C::*), PointerToMember, U>
{
	U&& m_value;

public:
	explicit assign_member_t(U&& value)
		: m_value(value)
	{
	}

	void set_argument_on(C& arguments) const&
	{
		(arguments.*PointerToMember) = m_value;
	}

	void set_argument_on(C& arguments) const&&
	{
		(arguments.*PointerToMember) = vsm_move(*this).m_value;
	}
};

template<auto Member, typename T>
auto assign_member(T&& value)
{
	return assign_member_t<decltype(Member), Member, T>(vsm_forward(value));
}


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
		else if constexpr (requires { arguments.set_argument(vsm_forward(new_argument)); })
		{
			arguments.set_argument(vsm_forward(new_argument));
		}
		else
		{
			vsm_forward(new_argument).set_argument_on(arguments);
		}
	}
};
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
