#pragma once

#include <algorithm>

namespace allio::detail::win32_path_traits_impl {

template<typename Char>
bool is_separator(Char const character)
{
	return character == static_cast<Char>('\\') || character == static_cast<Char>('/');
}

template<typename Char>
Char const* find_separator(Char const* const beg, Char const* const end)
{
	return std::find_if(beg, end, [](Char const character)
	{
		return is_separator(character);
	});
}

template<typename Char>
Char const* find_last_separator(Char const* const beg, Char const* const end)
{
	Char const* pos = end;
	while (pos != beg)
	{
		if (is_separator(*--pos))
		{
			return pos;
		}
	}
	return end;
}

template<typename Char>
Char const* skip_separators(Char const* const beg, Char const* const end)
{
	return std::find_if_not(beg, end, [](Char const character)
	{
		return is_separator(character);
	});
}

template<typename Char>
Char const* skip_last_separators(Char const* const beg, Char const* end)
{
	while (beg != end && is_separator(end[-1]))
	{
		--end;
	}
	return end;
}

template<typename Char>
bool is_drive_letter(Char const letter)
{
	return
		(static_cast<Char>('a') <= letter && letter <= static_cast<Char>('z')) ||
		(static_cast<Char>('A') <= letter && letter <= static_cast<Char>('Z'));
}

template<typename Char>
bool has_drive_letter(Char const* const beg, Char const* const end)
{
	return end - beg >= 2 && beg[1] == ':' && is_drive_letter(*beg);
}

} // namespace allio::detail::win32_path_traits_impl
