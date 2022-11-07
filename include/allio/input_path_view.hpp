#pragma once

#include <allio/input_string_view.hpp>

#include <allio/path.hpp>
#include <allio/path_view.hpp>

namespace allio {

struct input_path_view : input_string_view
{
	explicit constexpr input_path_view(input_string_view const string)
		: input_string_view(string)
	{
	}


	constexpr input_path_view(basic_path_literal<char> const path)
		: input_string_view(path.data(), path.size(), true)
	{
	}

	constexpr input_path_view(basic_path_view<char> const path)
		: input_string_view(path.string())
	{
	}

	constexpr input_path_view(basic_path<char> const& path)
		: input_string_view(path.string(), true)
	{
	}


#if allio_detail_PLATFORM_WIDE
	constexpr input_path_view(basic_path_literal<wchar_t> const path)
		: input_string_view(path.data(), path.size(), true)
	{
	}

	constexpr input_path_view(basic_path_view<wchar_t> const path)
		: input_string_view(path.string())
	{
	}

	constexpr input_path_view(basic_path<wchar_t> const& path)
		: input_string_view(path.string(), true)
	{
	}
#endif
};

} // namespace allio
