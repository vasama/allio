#pragma once

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

	using string_view_type::data;
	using string_view_type::size;
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

#if allio_detail_PLATFORM_WIDE
#	define allio_PATH(...) \
		L"" __VA_ARGS__ ""
#else
#	define allio_PATH(...) \
		"" __VA_ARGS__ ""
#endif
