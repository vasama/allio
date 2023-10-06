#pragma once

#include <vsm/concepts.hpp>
#include <vsm/standard.hpp>
#include <vsm/tag_invoke.hpp>
#include <vsm/utility.hpp>

namespace allio {

struct get_path_string_t
{
	template<typename Path>
	decltype(auto) vsm_static_operator_invoke(Path&& path)
		requires vsm::tag_invocable<get_path_string_t, Path&&>
	{
		return vsm::tag_invoke(get_path_string_t(), vsm_forward(path));
	}
};
inline constexpr get_path_string_t get_path_string = {};

} // namespace allio
