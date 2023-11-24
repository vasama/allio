#pragma once

#include <allio/detail/path.hpp>

#include <string_view>

namespace allio {

template<typename Char>
class basic_path_literal : std::basic_string_view<Char>
{
	using string_view_type = std::basic_string_view<Char>;

public:
	explicit constexpr basic_path_literal(Char const* const c_string, size_t const size)
		: string_view_type(c_string, size)
	{
	}

	friend string_view_type tag_invoke(get_path_string_t, basic_path_literal const& self)
	{
		return static_cast<string_view_type const&>(self);
	}
};

namespace path_literals {

constexpr basic_path_literal<char> operator""_path(char const* const c_string, size_t const size)
{
	return basic_path_literal<char>(c_string, size);
}

constexpr basic_path_literal<wchar_t> operator""_path(wchar_t const* const c_string, size_t const size)
{
	return basic_path_literal<wchar_t>(c_string, size);
}

} // namespace path_literals
} // namespace allio

#if allio_detail_platform_wide
#	define allio_path(...) L"" __VA_ARGS__ ""
#else
#	define allio_path(...) "" __VA_ARGS__ ""
#endif
