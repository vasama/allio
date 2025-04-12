#pragma once

#include <allio/path_view.hpp>

#include <vsm/concepts.hpp>
#include <vsm/type_traits.hpp>

#include <ranges>
#include <string>
#include <vector>

namespace allio::test {

using child_entry_point_t = int(*)(int argc, char const* const* argv);

namespace detail {

template<size_t... Indices>
decltype(auto) child_entry_point_invoke(
	auto&& get_argument,
	auto&& callable,
	std::index_sequence<Indices...>)
{
	return vsm_forward(callable)(get_argument(Indices)...);
}

template<size_t Size>
decltype(auto) child_entry_point_invoke(auto&& get_argument, auto&& callable)
{
	return detail::child_entry_point_invoke(
		vsm_forward(get_argument),
		vsm_forward(callable),
		std::make_index_sequence<Size>());
}


template<typename Callable, typename R, typename... Ps>
int child_entry_point_impl(int const argc, char const* const* const argv)
{
	if (argc != sizeof...(Ps))
	{
		return EXIT_FAILURE;
	}

	return detail::child_entry_point_invoke<sizeof...(Ps)>(
		[&](size_t const index) -> std::string_view
		{
			return argv[index];
		},
		[](std::same_as<std::string_view> auto const... args) -> int
		{
			if constexpr (std::is_same_v<R, void>)
			{
				Callable()(args...);
				return EXIT_SUCCESS;
			}
			else
			{
				return Callable()(args...);
			}
		});
}

template<typename Member>
struct to_child_entry_point_impl;

template<typename C, typename R, typename... Ps>
struct to_child_entry_point_impl<R(C:: *)(Ps...)>
{
	static constexpr child_entry_point_t entry_point = child_entry_point_impl<C, R, Ps...>;
};

template<typename C, typename R, typename... Ps>
struct to_child_entry_point_impl<R(C:: *)(Ps...) const>
{
	static constexpr child_entry_point_t entry_point = child_entry_point_impl<C, R, Ps...>;
};


inline child_entry_point_t to_child_entry_point(child_entry_point_t const entry_point)
{
	return entry_point;
}

template<typename Callable>
auto to_child_entry_point(Callable const&)
	-> decltype(to_child_entry_point_impl<decltype(&Callable::operator())>::entry_point)
{
	return to_child_entry_point_impl<decltype(&Callable::operator())>::entry_point;
}

template<typename EntryPoint>
concept child_entry_point = requires (EntryPoint&& entry_point)
{
	detail::to_child_entry_point(vsm_forward(entry_point));
};

} // namespace detail

path_view get_child_executable_path();

std::string get_entry_point_option(child_entry_point_t entry_point);

template<detail::child_entry_point EntryPoint, std::ranges::input_range Range>
	requires std::convertible_to<std::ranges::range_reference_t<Range>, std::string_view>
std::vector<std::string> make_child_args(EntryPoint&& entry_point, Range&& args)
{
	std::vector<std::string> args_vector;

	args_vector.push_back(get_entry_point_option(
		detail::to_child_entry_point(vsm_forward(entry_point))));

	for (std::string_view const argument : vsm_forward(args))
	{
		args_vector.emplace_back(argument);
	}

	return args_vector;
}

template<detail::child_entry_point EntryPoint, std::convertible_to<std::string_view>... Args>
std::vector<std::string> make_child_args(EntryPoint&& entry_point, Args&&... args)
{
	using array_type = std::string_view[sizeof...(Args)];
	return make_child_args(vsm_forward(entry_point), array_type{ vsm_forward(args)... });
}

} // namespace allio::test
