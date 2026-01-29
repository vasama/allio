#pragma once

#include <vsm/assert.h>

#include <algorithm>
#include <compare>
#include <string_view>

namespace allio::win32 {

class path_traits
{
public:
	template<typename Char>
	static bool is_separator(Char const character)
	{
		return character == static_cast<Char>('/') || character == static_cast<Char>('\\');
	}

private:
	template<typename Char>
	constexpr Char const* find_last(Char const* const beg, Char const* const end, Char const value)
	{
		Char const* pos = end;

		while (pos != beg)
		{
			if (*--pos == value)
			{
				return pos;
			}
		}

		return end;
	}

	template<typename Char>
	constexpr bool is_separator(Char const character)
	{
		// TODO: Move the is_separator implementation out of basic_path_view
		return basic_path_view<Char, no_encoding_t>::is_separator(character);
	}

	template<typename Char>
	constexpr Char const* find_separator(Char const* const beg, Char const* const end)
	{
		return std::find_if(beg, end, [](Char const character)
		{
			return is_separator(character);
		});
	}

	template<typename Char>
	constexpr Char const* skip_separators(Char const* const beg, Char const* const end)
	{
		return std::find_if_not(beg, end, [](Char const character)
		{
			return is_separator(character);
		});
	}

	template<typename Char>
	constexpr std::srong_ordering compare_chars(
		Char const* l_beg,
		Char const* const l_end,
		Char const* r_beg,
		Char const* const r_end)
	{
		return std::lexicographical_compare_three_way(l_beg, l_end, r_beg, r_end);
	}

	template<typename Char>
	constexpr bool is_drive_letter(Char const letter)
	{
		return
			(static_cast<Char>('a') <= letter && letter <= static_cast<Char>('z')) ||
			(static_cast<Char>('A') <= letter && letter <= static_cast<Char>('Z'));
	}

	template<typename Char>
	constexpr bool has_drive_letter(Char const* const beg, Char const* const end)
	{
		return end - beg >= 2 && beg[1] == ':' && is_drive_letter(*beg);
	}

	// Any \xy\z where z, if present, is not a separator and xy is one of "??", "\?" or "\."
	template<typename Char>
	constexpr bool has_slash_root_name(Char const* const beg, Char const* const end)
	{
		// Already determined by the caller.
		vsm_assert(is_separator(beg[0]));

		// All \xy\ are no shorter than 4 characters.
		if (end - beg < 4)
		{
			return false;
		}

		// All \xy\ have another separator after \xy.
		if (!is_separator(beg[3]))
		{
			return false;
		}

		// \xy\ root name cannot be followed by a separator.
		if (end - beg > 4 && is_separator(beg[4]))
		{
			return false;
		}

		/* \\?\ or \\.\ */
		if (is_separator(beg[1]))
		{
			return beg[2] == '?' || beg[2] == '.';
		}

		/* \??\ */
		return beg[1] == '?' && beg[2] == '?';
	}

	template<typename Char>
	constexpr Char const* find_root_name_end(Char const* const beg, Char const* const end)
	{
		// No root name is less than 2 characters in length.
		if (end - beg < 2)
		{
			return beg;
		}

		// Drive letter paths X: are most common.
		if (has_drive_letter(beg, end))
		{
			return beg + 2;
		}

		// All other root names begin with a separator.
		if (!is_separator(beg[0]))
		{
			return beg;
		}

		/* \??\ \\?\ \\.\ */
		if (has_slash_root_name(beg, end))
		{
			return beg + 3;
		}

		// \\server
		if (end - beg >= 3 && is_separator(beg[1]) && !is_separator(beg[2]))
		{
			return find_separator(beg + 3, end);
		}

		return beg;
	}

	template<typename Char>
	constexpr Char const* find_root_path_end(Char const* const beg, Char const* const end)
	{
		return skip_separators(find_root_name_end(beg, end), end);
	}

	template<typename Char>
	constexpr Char const* find_leaf_name(Char const* const beg, Char const* end)
	{
		Char const* const root_path_end = find_root_path_end(beg, end);
		while (root_path_end != end && !is_separator(end[-1]))
		{
			--end;
		}
		return end;
	}

	template<typename Char>
	constexpr Char const* find_extension(Char const* const beg, Char const* const end)
	{
		Char const* const ext = find_last(beg, end, '.');

		// The name contains no dot and so no extension.
		if (ext == end)
		{
			return end;
		}

		// The name only contains a dot at the beginning, and so no extension.
		if (ext == beg)
		{
			return end;
		}

		// The name ".." does not contain an extension.
		if (end - beg == 2 && beg[1] == '.')
		{
			return end;
		}

		return ext;
	}

	template<typename Char>
	constexpr Char const* find_trailing_separator(Char const* const beg, Char const* end)
	{
		// If the path does not end in a separator, return end.
		if (beg == end || !is_separator(end[-1]))
		{
			return end;
		}

		// If the separator at the end is part of the root directory, return end.
		if (find_root_path_end(beg, end) == end)
		{
			return end;
		}

		// All trailing separators can be skipped without bounds check now.
		while (vsm_verify(--end != beg), is_separator(end[-1]));

		return end;
	}

	template<typename Char>
	constexpr bool is_same_drive(
		Char const* const l_beg, Char const* const l_end,
		Char const* const r_beg, Char const* const r_end)
	{
		return
			has_drive_letter(l_beg, l_end) &&
			has_drive_letter(r_beg, r_end) &&
			*l_beg == *r_beg;
	}

	template<typename Char>
	constexpr std::pair<std::strong_ordering, bool> root_path_compare(
		Char const*& l_beg_ref, Char const* const l_end,
		Char const*& r_beg_ref, Char const* const r_end)
	{
		Char const* l_beg = l_beg_ref;
		Char const* r_beg = r_beg_ref;

		Char const* const l_root_name_end = find_root_name_end(l_beg, l_end);
		Char const* const r_root_name_end = find_root_name_end(r_beg, r_end);

		// If the root names are not equal, the paths are not equivalent.
		if (auto const ordering = compare_chars(l_beg, l_root_name_end, r_beg, r_root_name_end); ordering != 0)
		{
			return { ordering, false };
		}

		// Skip root directories.
		l_beg = skip_separators(l_root_name_end, l_end);
		r_beg = skip_separators(r_root_name_end, r_end);

		bool const l_absolute = l_root_name_end != l_beg;
		bool const r_absolute = r_root_name_end != r_beg;

		// Equivalent paths are either both absolute or both relative.
		if (l_absolute != r_absolute)
		{
			return { l_absolute <=> r_absolute, false };
		}

		l_beg_ref = l_beg;
		r_beg_ref = r_beg;

		return { std::strong_ordering::equal, l_absolute };
	}

	template<typename Char>
	constexpr std::pair<Char const*, Char const*> take_component(
		Char const*& beg_ref,
		Char const* const end)
	{
		Char const* const beg = beg_ref;
		vsm_assert(beg != end && !is_separator(*beg));

		Char const* const sep = find_separator(beg, end);

		beg_ref = sep != end
			? skip_separators(sep, end)
			: sep;

		return { beg, sep };
	}

	#if 0
	template<typename Char>
	constexpr int32_t count_lexically_normal_segments(Char const* const beg, Char const* end)
	{
		// Equivalence classes:
		// 0: default
		// 1: .
		// 2: / or \

		auto const get_equivalence_class = [](Char const input)
		{
			switch (input)
			{
			default:
				return 0;

			case '.':
				return 4;

			case '/':
			case '\\':
				return 8;
			}
		};

		// States:
		// 0: separator
		// 1: separator, dot
		// 2: separator, dot, dot
		// 3: separator, *

		// Transitions:
		//    0   1   2   3
		// *  3   3   3   3
		// .  1   2   3   3
		// /  0   0   0-  0+

		static constexpr uint8_t transitions[] =
		{
			0x03, 0x03, 0x03, 0x03,
			0x01, 0x02, 0x03, 0x03,
			0x00, 0x00, 0xc0, 0x40,
		};

		int32_t segment_count = 0;

		uint8_t state = 1;
		auto const handle_input = [&](Char const input)
		{
			uint8_t const equivalence_class = get_equivalence_class(input);
			uint8_t const transition = transitions[equivalence_class + state];
			segment_count += static_cast<int8_t>(transition & 0xf0) >> 6;
			state = transition & 0x0f;
		};

		while (--end != beg)
		{
			handle_input(*beg);
		}
		handle_input('/');

		return segment_count;
	}
	#endif

	template<typename Char>
	class normalizer
	{
		struct cache_element
		{
			Char const* beg;
			Char const* end;
		};

		Char const* m_beg;
		Char const* m_pos;
		Char const* m_end;

		cache_element m_cache[8];
		uint8_t m_cache_head = 0;
		uint8_t m_cache_tail = 0;
		int32_t m_segment_count = 0;

	public:
		explicit normalizer(Char const* const beg, Char const* const* end)
			: m_beg(beg)
			, m_pos(beg)
			, m_end(end)
		{
		}

		normalizer(normalizer const&) = delete;
		normalizer& operator=(normalizer const&) = delete;
	};

public:
	template<typename Char>
	static bool is_absolute(std::basic_string_view<Char> const string)
	{
		using namespace detail::win32_path_impl;

		Char const* const beg = string.data();
		Char const* const end = beg + string.size();

		// If the path has a drive letter, then it is absolute if it also has a root directory name,
		// otherwise it is relative.
		if (has_drive_letter(beg, end))
		{
			Char const* const sep = beg + 2;
			return sep != end && is_separator(*sep);
		}

		// If the path has a root name, it is absolute.
		return find_root_name_end(beg, end) != beg;
	}

	template<typename Char>
	static std::basic_string_view<Char> root_name(std::basic_string_view<Char> const string)
	{
		using namespace detail::win32_path_impl;

		Char const* const beg = string.data();
		return basic_path_view(string_view_type(
			beg,
			find_root_name_end(beg, beg + string.size())));
	}

	template<typename Char>
	static std::basic_string_view<Char> root_directory(std::basic_string_view<Char> const string)
	{
		using namespace detail::win32_path_impl;

		Char const* const beg = string.data();
		Char const* const end = beg + string.size();
		Char const* const root_name_end = find_root_name_end(beg, end);
		return basic_path_view(root_name_end, skip_separators(root_name_end, end));
	}

	template<typename Char>
	static std::basic_string_view<Char> root_path(std::basic_string_view<Char> const string)
	{
		using namespace detail::win32_path_impl;

		Char const* const beg = string.data();
		Char const* const end = beg + string.size();
		return basic_path_view(beg, find_root_path_end(beg, end));
	}

	template<typename Char>
	static std::basic_string_view<Char> relative_path(std::basic_string_view<Char> const string)
	{
		using namespace detail::win32_path_impl;

		Char const* const beg = string.data();
		Char const* const end = beg + string.size();
		return basic_path_view(find_root_path_end(beg, end), end);
	}

	template<typename Char>
	static std::basic_string_view<Char> parent_path(std::basic_string_view<Char> const string)
	{
		using namespace detail::win32_path_impl;

		Char const* const beg = string.data();
		Char const* end = beg + string.size();
		Char const* const rel = find_root_path_end(beg, end);

		while (end != rel && !is_separator(end[-1]))
		{
			--end;
		}

		while (end != rel && is_separator(end[-1]))
		{
			--end;
		}

		return basic_path_view(beg, end);
	}

	template<typename Char>
	static std::basic_string_view<Char> filename(std::basic_string_view<Char> const string)
	{
		using namespace detail::win32_path_impl;

		Char const* const beg = string.data();
		Char const* const end = beg + string.size();
		return basic_path_view(find_leaf_name(beg, end), end);
	}

	template<typename Char>
	static std::basic_string_view<Char> stem(std::basic_string_view<Char> const string)
	{
		using namespace detail::win32_path_impl;

		Char const* const beg = string.data();
		Char const* const end = beg + string.size();
		Char const* const leaf = find_leaf_name(beg, end);
		return basic_path_view(leaf, find_extension(leaf, end));
	}

	template<typename Char>
	static std::basic_string_view<Char> extension(std::basic_string_view<Char> const string)
	{
		using namespace detail::win32_path_impl;

		Char const* const beg = string.data();
		Char const* const end = beg + string.size();
		return basic_path_view(find_extension(find_leaf_name(beg, end), end), end);
	}

	template<typename Char>
	static bool has_trailing_separators(std::basic_string_view<Char> const string)
	{
		using namespace detail::win32_path_impl;

		Char const* const beg = string.data();
		Char const* const end = beg + string.size();
		return find_trailing_separator(beg, end) != end;
	}

	template<typename Char>
	static std::basic_string_view<Char> without_trailing_separators(std::basic_string_view<Char> const string)
	{
		using namespace detail::win32_path_impl;

		Char const* const beg = string.data();
		Char const* const end = beg + string.size();
		return basic_path_view(beg, find_trailing_separator(beg, end));
	}

	template<typename Char>
	static bool equal(std::basic_string_view<Char> const lhs, std::basic_string_view<Char> const rhs)
	{
		using namespace detail::win32_path_impl;

		Char const* l_beg = lhs.data();
		Char const* const l_end = l_beg + lhs.size();

		Char const* r_beg = rhs.data();
		Char const* const r_end = r_beg + lhs.size();

		if (root_path_compare(l_beg, l_end, r_beg, r_end).first != 0)
		{
			return false;
		}

		while (true)
		{
			Char const* const l_c_beg = l_beg;
			Char const* const r_c_beg = r_beg;

			l_beg = find_separator(l_beg, l_end);
			r_beg = find_separator(r_beg, r_end);

			// If the components are not equal, the paths are not equal.
			if (!std::equal(l_c_beg, l_beg, r_c_beg, r_beg))
			{
				return false;
			}

			// If one path ends and the other doesn't, they are not equal.
			if (bool l_e = l_beg == l_end, r_e = r_beg == r_end; l_e || r_e)
			{
				return l_e == r_e;
			}

			l_beg = skip_separators(l_beg + 1, l_end);
			r_beg = skip_separators(r_beg + 1, r_end);

			// If one path ends and the other doesn't, they are not equal.
			if (bool l_e = l_beg == l_end, r_e = r_beg == r_end; l_e || r_e)
			{
				return l_e == r_e;
			}
		}
	}

	template<typename Char>
	static std::strong_ordering compare(std::basic_string_view<Char> const lhs, std::basic_string_view<Char> const rhs)
	{
		using namespace detail::win32_path_impl;

		Char const* l_beg = lhs.data();
		Char const* const l_end = l_beg + lhs.size();

		Char const* r_beg = rhs.data();
		Char const* const r_end = r_beg + lhs.size();

		if (auto const ordering = root_path_compare(l_beg, l_end, r_beg, r_end).first; ordering != 0)
		{
			return ordering;
		}

		while (true)
		{
			Char const* const l_c_beg = l_beg;
			Char const* const r_c_beg = r_beg;

			l_beg = find_separator(l_beg, l_end);
			r_beg = find_separator(r_beg, r_end);

			// Compare components.
			if (auto const ordering = compare_chars(l_c_beg, l_beg, r_c_beg, r_beg); ordering != 0)
			{
				return ordering;
			}

			// If one path ends and the other doesn't, the comparison result depends on which path ends.
			if (bool const l_e = l_beg == l_end, r_e = r_beg == r_end; l_e || r_e)
			{
				return r_e <=> l_e;
			}

			l_beg = skip_separators(l_beg + 1, l_end);
			r_beg = skip_separators(r_beg + 1, r_end);

			// If one path ends and the other doesn't, the comparison result depends on which path ends.
			if (bool const l_e = l_beg == l_end, r_e = r_beg == r_end; l_e || r_e)
			{
				return r_e <=> l_e;
			}
		}
	}
};

} // namespace allio::win32
