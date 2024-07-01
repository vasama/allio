#pragma once

#include <allio/any_string.hpp>
#include <allio/path_view.hpp>

#include <vsm/lift.hpp>

namespace allio {
namespace detail {

template<typename Path>
concept _any_path = _any_string<decltype(get_path_string(std::declval<Path const&>()))>;

} // namespace detail

class any_path_view : any_string_view
{
public:
	any_path_view() = default;

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
		return any_string_view::visit(vsm_bind_borrow(_visitor, vsm_forward(visitor)));
	}

private:
	template<typename Visitor, typename Char, std::same_as<null_terminated_t>... Tag>
	[[nodiscard]] decltype(auto) _visitor(Visitor&& visitor, std::basic_string_view<Char> const string, Tag...) const
	{
		if constexpr (std::invocable<Visitor&&, basic_path_view<Char>, Tag...>)
		{
			return vsm_forward(visitor)(basic_path_view<Char>(string), Tag()...);
		}
		else
		{
			return vsm_forward(visitor)(basic_path_view<Char>(string));
		}
	}

	template<typename Visitor>
	[[nodiscard]] decltype(auto) _visitor(Visitor&& visitor, detail::string_length_out_of_range_t)
	{
		return vsm_forward(visitor)(detail::string_length_out_of_range_t());
	}
};

} // namespace allio
