#pragma once

#include <vsm/assert.h>

#include <algorithm>

#include <allio/linux/detail/undef.i>

namespace allio {
namespace detail::path_impl {

template<typename Char>
constexpr bool is_separator(Char const character)
{
	return basic_path_view<Char, no_encoding_t>::is_separator(character);
}

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

} // namespace detail::path_impl


template<typename Char, typename Encoding>
constexpr bool basic_path_view<Char, Encoding>::is_absolute() const
{
	using namespace detail::path_impl;

	Char const* const beg = m_string_view.data();
	Char const* const end = beg + m_string_view.size();

	// If the path has a root name, it is absolute.
	return starts_with_separator(beg, end);
}


template<typename Char, typename Encoding>
constexpr basic_path_view<Char, Encoding> basic_path_view<Char, Encoding>::root_name() const
{
	return basic_path_view<Char>();
}

template<typename Char, typename Encoding>
constexpr basic_path_view<Char, Encoding> basic_path_view<Char, Encoding>::root_directory() const
{
	return root_path();
}

template<typename Char, typename Encoding>
constexpr basic_path_view<Char, Encoding> basic_path_view<Char, Encoding>::root_path() const
{
	using namespace detail::path_impl;

	Char const* const beg = m_string_view.data();
	Char const* const end = beg + m_string_view.size();
	return basic_path_view(string_view_type(beg, starts_with_separator(beg, end)));
}

template<typename Char, typename Encoding>
constexpr basic_path_view<Char, Encoding> basic_path_view<Char, Encoding>::relative_path() const
{
	using namespace detail::path_impl;

	Char const* const beg = m_string_view.data();
	Char const* const end = beg + m_string_view.size();
	Char const* const rel = skip_separators(beg, end);
	return basic_path_view(string_view_type(rel, end));
}

template<typename Char, typename Encoding>
constexpr basic_path_view<Char, Encoding> basic_path_view<Char, Encoding>::parent_path() const
{
	using namespace detail::path_impl;

	Char const* const beg = m_string_view.data();
	Char const* end = beg + m_string_view.size();
	Char const* const rel = skip_separators(beg, end);

	while (end != rel && !is_separator(end[-1]))
	{
		--end;
	}

	while (end != rel && is_separator(end[-1]))
	{
		--end;
	}

	return basic_path_view(string_view_type(beg, end));
}

template<typename Char, typename Encoding>
constexpr basic_path_view<Char, Encoding> basic_path_view<Char, Encoding>::filename() const
{
	using namespace detail::path_impl;

	Char const* const beg = m_string_view.data();
	Char const* const end = beg + m_string_view.size();
	Char const* const leaf = find_leaf_name(beg, end);
	return basic_path_view(string_view_type(leaf, end));
}

template<typename Char, typename Encoding>
constexpr basic_path_view<Char, Encoding> basic_path_view<Char, Encoding>::stem() const
{
	using namespace detail::path_impl;

	Char const* const beg = m_string_view.data();
	Char const* const end = beg + m_string_view.size();
	Char const* const leaf = find_leaf_name(beg, end);
	Char const* const ext = find_extension(leaf, end);
	return basic_path_view(string_view_type(leaf, ext));
}

template<typename Char, typename Encoding>
constexpr basic_path_view<Char, Encoding> basic_path_view<Char, Encoding>::extension() const
{
	using namespace detail::path_impl;

	Char const* const beg = m_string_view.data();
	Char const* const end = beg + m_string_view.size();
	Char const* const leaf = find_leaf_name(beg, end);
	Char const* const ext = find_extension(leaf, end);
	return basic_path_view(string_view_type(ext, end));
}


template<typename Char, typename Encoding>
constexpr bool basic_path_view<Char, Encoding>::has_trailing_separators() const
{
	using namespace detail::path_impl;

	Char const* const beg = m_string_view.data();
	Char const* const end = beg + m_string_view.size();
	return find_trailing_separator(beg, end) != end;
}

template<typename Char, typename Encoding>
constexpr basic_path_view<Char, Encoding> basic_path_view<Char, Encoding>::without_trailing_separators() const
{
	using namespace detail::path_impl;

	Char const* const beg = m_string_view.data();
	Char const* const end = beg + m_string_view.size();
	return basic_path_view(string_view_type(beg, find_trailing_separator(beg, end)));
}


template<typename Char, typename Encoding>
constexpr bool basic_path_view<Char, Encoding>::equal(basic_path_view const lhs, basic_path_view const rhs)
{
	using namespace detail::path_impl;

	string_view_type const lhs_string = lhs.string();
	string_view_type const rhs_string = rhs.string();

	Char const* beg1 = lhs_string.data();
	Char const* const end1 = beg1 + lhs_string.size();

	Char const* beg2 = rhs_string.data();
	Char const* const end2 = beg2 + lhs_string.size();

	if (root_path_compare(beg1, end1, beg2, end2).first != 0)
	{
		return false;
	}

	while (true)
	{
		Char const* const cbeg1 = beg1;
		Char const* const cbeg2 = beg2;

		beg1 = find_separator(beg1, end1);
		beg2 = find_separator(beg2, end2);

		// If the components are not equal, the paths are not equal.
		if (!std::equal(cbeg1, beg1, cbeg2, beg2))
		{
			return false;
		}

		// If one path ends and the other doesn't, they are not equal.
		if (bool e1 = beg1 == end1, e2 = beg2 == end2; e1 || e2)
		{
			return e1 == e2;
		}

		beg1 = skip_separators(beg1 + 1, end1);
		beg2 = skip_separators(beg2 + 1, end2);

		// If one path ends and the other doesn't, they are not equal.
		if (bool e1 = beg1 == end1, e2 = beg2 == end2; e1 || e2)
		{
			return e1 == e2;
		}
	}
}

template<typename Char, typename Encoding>
constexpr std::strong_ordering basic_path_view<Char, Encoding>::compare(
	basic_path_view const lhs,
	basic_path_view const rhs)
{
	using namespace detail::path_impl;

	string_view_type const lhs_string = lhs.string();
	string_view_type const rhs_string = rhs.string();

	Char const* beg1 = lhs_string.data();
	Char const* const end1 = beg1 + lhs_string.size();

	Char const* beg2 = rhs_string.data();
	Char const* const end2 = beg2 + lhs_string.size();

	if (auto const ordering = root_path_compare(beg1, end1, beg2, end2).first)
	{
		return ordering;
	}

	while (true)
	{
		Char const* const cbeg1 = beg1;
		Char const* const cbeg2 = beg2;

		beg1 = find_separator(beg1, end1);
		beg2 = find_separator(beg2, end2);

		// Compare components.
		if (auto const ordering = compare_chars(cbeg1, beg1, cbeg2, beg2))
		{
			return ordering;
		}

		// If one path ends and the other doesn't, the comparison result depends on which path ends.
		if (bool const e1 = beg1 == end1, e2 = beg2 == end2; e1 || e2)
		{
			return e2 <=> e1;
		}

		beg1 = skip_separators(beg1 + 1, end1);
		beg2 = skip_separators(beg2 + 1, end2);

		// If one path ends and the other doesn't, the comparison result depends on which path ends.
		if (bool const e1 = beg1 == end1, e2 = beg2 == end2; e1 || e2)
		{
			return e2 <=> e1;
		}
	}
}


template<typename Char, typename Encoding>
constexpr void basic_path_view<Char, Encoding>::iterator::init_begin(Char const* beg, Char const* end)
{
	using namespace detail::path_impl;

	m_beg = beg;
	m_end = end;

	if (Char const* const rel = skip_separators(beg, end); beg != rel)
	{
		m_sbeg = rel - 1;
		m_send = rel;
	}
	else
	{
		m_send = find_separator(beg, end);
	}
}

template<typename Char, typename Encoding>
constexpr void basic_path_view<Char, Encoding>::iterator::increment()
{
	using namespace detail::path_impl;

	// Let N+0 denote the component currently referred to by this iterator.
	// Let N+1 denote the next component which is the logical result of this function.

	Char const* const end = m_end;
	Char const* sbeg = m_sbeg;

	vsm_assert(sbeg != end);
	if (is_separator(*sbeg) && sbeg == m_send)
	{
		m_sbeg = sbeg + 1;
		m_send = sbeg + 1;
		return;
	}

	if ((sbeg = m_send) == end)
	{
		m_sbeg = end;
		m_send = end;
		return;
	}

	// There may be multiple separators.
	sbeg = skip_separators(sbeg, end);

	// Magic empty path at the end.
	if (sbeg == end)
	{
		m_sbeg = sbeg - 1;
		m_send = sbeg - 1;
		return;
	}

	// Regular path component.
	m_sbeg = sbeg;
	m_send = find_separator(sbeg, end);
}

template<typename Char, typename Encoding>
constexpr void basic_path_view<Char, Encoding>::iterator::decrement()
{
	using namespace detail::path_impl;

	// Let N+0 denote the component currently referred to by this iterator.
	// Let N+1 denote the next component which is the logical result of this function.

	Char const* const beg = m_beg;
	Char const* const end = m_end;

	Char const* const rel = skip_separators(beg, end);

	Char const* sbeg = m_sbeg;

	// N-1 is the root directory.
	if (sbeg == rel)
	{
		m_sbeg = rel - 1;
		m_send = rel;
		return;
	}

	// N-1 is the magic empty path at the end.
	if (sbeg == end && is_separator(sbeg[-1]))
	{
		m_sbeg = sbeg - 1;
		m_send = sbeg - 1;
		return;
	}

	// Skip separators.
	while (sbeg != rel && is_separator(sbeg[-1]))
	{
		--sbeg;
	}

	// Save N-1 end pointer.
	Char const* const send = sbeg;

	// Skip until next separator.
	while (sbeg != rel && !is_separator(sbeg[-1]))
	{
		--sbeg;
	}

	m_sbeg = sbeg;
	m_send = send;
}


template<typename Char, typename Encoding>
constexpr basic_path_combine_result<Char, Encoding> allio::combine_path(
	basic_path_view<Char, Encoding> const lhs,
	basic_path_view<Char, Encoding> const rhs)
{
	using namespace detail::path_impl;

	using path_view_type = basic_path_view<Char>;
	using string_view_type = std::basic_string_view<Char>;
	using result_type = basic_path_combine_result<Char>;

	string_view_type const l_string = lhs.string();
	Char const* const l_beg = l_string.data();
	Char const* const l_end = l_beg + l_string.size();

	// If lhs is empty, rhs is returned.
	if (l_beg == l_end)
	{
		return result_type(rhs);
	}

	string_view_type const r_string = rhs.string();
	Char const* const r_beg = r_string.data();
	Char const* const r_end = r_beg + r_string.size();

	// If rhs is absolute, it is returned and lhs discarded.
	if (starts_with_separator(r_beg, r_end))
	{
		return result_type(rhs);
	}

	// An added separator is be required if lhs does not end in a separator.
	bool const requires_separator = !path_view_type::is_separator(l_end[-1]);

	// Otherwise the result is both paths combined,
	// with a separator in between if lhs does not end with a separator.
	return result_type(lhs, rhs, requires_separator);
}

} // namespace allio

#include <allio/linux/detail/undef.i>
