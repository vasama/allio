#pragma once

#include <allio/any_string.hpp>
#include <allio/path_view.hpp>

namespace allio {
namespace detail {

template<typename Path>
concept _any_path = _any_string<decltype(get_path_string(std::declval<Path const&>()))>;

} // namespace detail

class any_path_view : any_string_view
{
public:
	template<detail::_any_path Path>
	any_path_view(Path const& path)
		: any_string_view(get_path_string(path))
	{
	}


	using any_string_view::empty;
	using any_string_view::size;
	using any_string_view::data;
	using any_string_view::is_null_terminated;


	[[nodiscard]] any_string_view string() const
	{
		return *this;
	}

	template<typename Visitor>
	[[nodiscard]] decltype(auto) visit(Visitor&& visitor) const
	{
		return any_string_view::_visit<false>(
			[&]<typename Char, typename... Tag>(std::basic_string_view<Char> const string, Tag... tag)
			{
				if constexpr (std::invocable<Visitor&&, basic_path_view<Char>, Tag...>)
				{
					return std::invoke(vsm_forward(visitor), basic_path_view<Char>(string), tag...);
				}
				else
				{
					return std::invoke(vsm_forward(visitor), basic_path_view<Char>(string));
				}
			}
		);
	}
};

} // namespace allio
