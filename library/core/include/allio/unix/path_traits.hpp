#pragma once

#include <vsm/assert.h>

#include <algorithm>
#include <compare>
#include <string_view>

namespace allio::unix {

class path_traits
{
public:
	template<typename Char>
	static bool is_separator(Char const character)
	{
		return character == static_cast<Char>('/');
	}

private:
	template<typename Char>
	constexpr Char const* find_separator(Char const* const beg, Char const* const end)
	{
		return std::find(beg, end, static_cast<Char>('/'));
	}

	template<typename Char>
	constexpr Char const* skip_separators(Char const* const beg, Char const* const end)
	{
		return std::find_if(beg, end, [](Char const character)
		{
			return character != static_cast<Char>('/');
		});
	}

	template<typename Char>
	constexpr std::strong_ordering compare_chars(
		Char const* l_beg,
		Char const* const l_end,
		Char const* r_beg,
		Char const* const r_end)
	{
		return std::lexicographical_compare_three_way(l_beg, l_end, r_beg, r_end);
	}

	template<typename Char>
	constexpr bool starts_with_separator(Char const* const beg, Char const* const end)
	{
		return beg != end && is_separator(*beg);
	}

	template<typename Char>
	constexpr Char const* find_leaf_name(Char const* const beg, Char const* end)
	{
		Char const* const rel = skip_separators(beg, end);
		while (rel != end && !is_separator(end[-1]))
		{
			--end;
		}
		return end;
	}

	template<typename Char>
	constexpr Char const* find_extension(Char const* const beg, Char const* const end)
	{
		Char const* ext = end;

		// If path is empty or a single character, there is no extension.
		if (ext == beg || --ext == beg)
		{
			return end;
		}

		if (*ext == '.')
		{
			// If the previous character is at the beginning and is a dot, so there is no extension.
			if (Char const* prev = ext - 1; prev == beg && *prev == '.')
			{
				return end;
			}

			// There is a dot not at the beginning of the name, so there is an extension.
			return ext;
		}

		// Scan for a dot not at the beginning of the name, which starts an extension.
		while (--ext != beg)
		{
			if (*ext == '.')
			{
				return ext;
			}
		}

		// Reached the beginning of the name, so there is no extension.
		return end;
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
		if (skip_separators(beg, end) == end)
		{
			return end;
		}

		// All trailing separators can be skipped without bounds check now.
		while (vsm_verify(--end != beg), is_separator(end[-1]));

		return end;
	}

	template<typename Char>
	constexpr std::pair<std::strong_ordering, bool> root_path_compare(
		Char const*& l_beg_ref, Char const* const l_end,
		Char const*& r_beg_ref, Char const* const r_end)
	{
		Char const* const l_beg = l_beg_ref;
		Char const* const r_beg = r_beg_ref;

		Char const* const l_rel = skip_separators(l_beg, l_end);
		Char const* const r_rel = skip_separators(r_beg, r_end);

		bool const l_absolute = l_rel != l_beg;
		bool const r_absolute = r_rel != r_beg;

		// Equivalent paths are either both absolute or both relative.
		if (l_absolute != r_absolute)
		{
			return { l_absolute <=> r_absolute, false };
		}

		l_beg_ref = l_rel;
		r_beg_ref = r_rel;

		return { std::strong_ordering::equal, l_absolute };
	}

public:
	template<typename Char>
	static bool is_absolute(std::basic_string_view<Char> const string)
	{
		using namespace detail::unix_path_impl;

		Char const* const beg = string.data();
		Char const* const end = beg + string.size();

		// If the path has a root name, it is absolute.
		return starts_with_separator(beg, end);
	}

	template<typename Char>
	static std::basic_string_view<Char> root_name(std::basic_string_view<Char> const string)
	{
		return {};
	}

	template<typename Char>
	static std::basic_string_view<Char> root_directory(std::basic_string_view<Char> const string)
	{
		return root_path();
	}

	template<typename Char>
	static std::basic_string_view<Char> root_path(std::basic_string_view<Char> const string)
	{
		using namespace detail::unix_path_impl;

		Char const* const beg = string.data();
		Char const* const end = beg + string.size();
		return basic_path_view(beg, starts_with_separator(beg, end));
	}

	template<typename Char>
	static std::basic_string_view<Char> relative_path(std::basic_string_view<Char> const string)
	{
		using namespace detail::unix_path_impl;

		Char const* const beg = string.data();
		Char const* const end = beg + string.size();
		return basic_path_view(skip_separators(beg, end), end);
	}

	template<typename Char>
	static std::basic_string_view<Char> parent_path(std::basic_string_view<Char> const string)
	{
		using namespace detail::unix_path_impl;

		Char const* const beg = string.data();
		Char const* end = beg + string.size();
		Char const* const rel = skip_separators(beg, end);

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
		using namespace detail::unix_path_impl;

		Char const* const beg = string.data();
		Char const* const end = beg + string.size();
		return basic_path_view(find_leaf_name(beg, end), end);
	}

	template<typename Char>
	static std::basic_string_view<Char> stem(std::basic_string_view<Char> const string)
	{
		using namespace detail::unix_path_impl;

		Char const* const beg = string.data();
		Char const* const end = beg + string.size();
		Char const* const leaf = find_leaf_name(beg, end);
		return basic_path_view(leaf, find_extension(leaf, end));
	}

	template<typename Char>
	static std::basic_string_view<Char> extension(std::basic_string_view<Char> const string)
	{
		using namespace detail::unix_path_impl;

		Char const* const beg = string.data();
		Char const* const end = beg + string.size();
		return basic_path_view(find_extension(find_leaf_name(beg, end), end), end);
	}

	template<typename Char>
	static bool has_trailing_separators(std::basic_string_view<Char> const string)
	{
		using namespace detail::unix_path_impl;

		Char const* const beg = string.data();
		Char const* const end = beg + string.size();
		return find_trailing_separator(beg, end) != end;
	}

	template<typename Char>
	static std::basic_string_view<Char> without_trailing_separators(std::basic_string_view<Char> const string)
	{
		using namespace detail::unix_path_impl;

		Char const* const beg = string.data();
		Char const* const end = beg + string.size();
		return basic_path_view(beg, find_trailing_separator(beg, end));
	}

	template<typename Char>
	static bool equal(std::basic_string_view<Char> const lhs, std::basic_string_view<Char> const rhs)
	{
		using namespace detail::unix_path_impl;

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
	static bool compare(std::basic_string_view<Char> const lhs, std::basic_string_view<Char> const rhs)
	{
		using namespace detail::unix_path_impl;

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

} // namespace allio::unix
