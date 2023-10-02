#pragma once

#include <vsm/tag_invoke.hpp>
#include <vsm/utility.hpp>

namespace allio::detail {

struct set_argument_t
{
	template<typename Parameters, typename Argument>
	void vsm_static_operator_invoke(Parameters& arguments, Argument&& argument)
		requires vsm::tag_invocable<set_argument_t, Parameters&, Argument&&>
	{
		vsm::tag_invoke(set_argument_t(), arguments, vsm_forward(argument));
	}
};
inline constexpr set_argument_t set_argument = {};

template<typename Parameters>
inline Parameters set_arguments(Parameters& arguments, auto&&... args)
{
	(set_argument(arguments, vsm_forward(args)), ...);
}

template<typename... Parameters>
struct parameters_t : Parameters... {};

struct no_parameters_t {};

} // namespace allio::detail
