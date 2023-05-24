#pragma once

#include <vsm/assert.h>

#include <algorithm>

namespace allio {
namespace detail::path_impl {

template<typename Char>
constexpr bool is_separator(Char const character)
{
	return basic_path_view<Char>::is_separator(character);
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
constexpr int compare_chars(Char const* l_beg, Char const* const l_end, Char const* r_beg, Char const* const r_end)
{
	size_t const size1 = l_end - l_beg;
	size_t const size2 = r_end - r_beg;

	for (size_t i = 0, c = std::min(size1, size2); i < c; ++i)
	{
		//TODO: how valid is this in the presence of unsigned character types?
		if (int const cmp = l_beg[i] - r_beg[i])
		{
			return cmp;
		}
	}

	return size1 - size2;
}


template<typename Char>
constexpr bool is_drive_letter(Char const letter)
{
	return
		static_cast<Char>('a') <= letter && letter <= static_cast<Char>('z') ||
		static_cast<Char>('A') <= letter && letter <= static_cast<Char>('Z');
}

template<typename Char>
constexpr bool has_drive_letter(Char const* const beg, Char const* const end)
{
	return end - beg >= 2 && beg[1] == ':' && is_drive_letter(*beg);
}

template<typename Char>
constexpr Char const* find_root_name_end(Char const* const beg, Char const* const end)
{
	// Single character path cannot have a root name.
	if (end - beg <= 1)
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

	// Any \xy\z where z, if present, is not a separator and xy is one of ?? or \? or \.
	if (end - beg >= 4 && is_separator(beg[3]) &&
		(end - beg == 4 || !is_separator(beg[4])) && (
			(is_separator(beg[1]) && (beg[2] == '?' || beg[2] == '.')) ||
			(beg[1] == '?' && beg[2] == '?')))
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
	while (end != root_path_end && !is_separator(end[-1]))
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
constexpr std::pair<int, bool> root_path_compare(
	Char const*& l_beg_ref, Char const* const l_end,
	Char const*& r_beg_ref, Char const* const r_end)
{
	Char const* l_beg = l_beg_ref;
	Char const* r_beg = r_beg_ref;

	Char const* const l_root_name_end = find_root_name_end(l_beg, l_end);
	Char const* const r_root_name_end = find_root_name_end(r_beg, r_end);

	// If the root names are not equal, the paths are not equivalent.
	if (int const cmp = compare_chars(l_beg, l_root_name_end, r_beg, r_root_name_end))
	{
		return { cmp, false };
	}

	// Skip root directories.
	l_beg = skip_separators(l_root_name_end, l_end);
	r_beg = skip_separators(r_root_name_end, r_end);

	bool const l_absolute = l_root_name_end != l_beg;
	bool const r_absolute = r_root_name_end != r_beg;

	// Equivalent paths are either both absolute or both relative.
	if (l_absolute != r_absolute)
	{
		return { static_cast<int>(l_absolute) - static_cast<int>(r_absolute), false };
	}

	l_beg_ref = l_beg;
	r_beg_ref = r_beg;

	return { 0, l_absolute };
}

template<typename Char>
constexpr std::pair<Char const*, Char const*> take_component(Char const*& beg_ref, Char const* const end)
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

} // namespace detail::path_impl


template<typename Char>
constexpr bool basic_path_view<Char>::is_absolute() const
{
	using namespace detail::path_impl;

	Char const* const beg = string_view_type::data();
	Char const* const end = beg + string_view_type::size();

	// If the path has a drive letter, then it is absolute if
	// it also has a directory name, otherwise it is relative.
	if (has_drive_letter(beg, end))
	{
		Char const* const sep = beg + 2;
		return sep != end && is_separator(*sep);
	}

	// If the path has a root name, it is absolute.
	return find_root_name_end(beg, end) != beg;
}


template<typename Char>
constexpr basic_path_view<Char> basic_path_view<Char>::root_name() const
{
	using namespace detail::path_impl;

	Char const* const beg = string_view_type::data();
	return basic_path_view(string_view_type(beg,
		find_root_name_end(beg, beg + string_view_type::size())));
}

template<typename Char>
constexpr basic_path_view<Char> basic_path_view<Char>::root_directory() const
{
	using namespace detail::path_impl;

	Char const* const beg = string_view_type::data();
	Char const* const end = beg + string_view_type::size();
	Char const* const root_name_end = find_root_name_end(beg, end);
	return basic_path_view(root_name_end, skip_separators(root_name_end, end));
}

template<typename Char>
constexpr basic_path_view<Char> basic_path_view<Char>::root_path() const
{
	using namespace detail::path_impl;

	Char const* const beg = string_view_type::data();
	Char const* const end = beg + string_view_type::size();
	return basic_path_view(beg, find_root_path_end(beg, end));
}

template<typename Char>
constexpr basic_path_view<Char> basic_path_view<Char>::relative_path() const
{
	using namespace detail::path_impl;

	Char const* const beg = string_view_type::data();
	Char const* const end = beg + string_view_type::size();
	return basic_path_view(find_root_path_end(beg, end), end);
}

template<typename Char>
constexpr basic_path_view<Char> basic_path_view<Char>::parent_path() const
{
	using namespace detail::path_impl;

	Char const* const beg = string_view_type::data();
	Char const* end = beg + string_view_type::size();
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
constexpr basic_path_view<Char> basic_path_view<Char>::filename() const
{
	using namespace detail::path_impl;

	Char const* const beg = string_view_type::data();
	Char const* const end = beg + string_view_type::size();
	return basic_path_view(find_leaf_name(beg, end), end);
}

template<typename Char>
constexpr basic_path_view<Char> basic_path_view<Char>::stem() const
{
	using namespace detail::path_impl;

	Char const* const beg = string_view_type::data();
	Char const* const end = beg + string_view_type::size();
	Char const* const leaf = find_leaf_name(beg, end);
	return basic_path_view(leaf, find_extension(leaf, end));
}

template<typename Char>
constexpr basic_path_view<Char> basic_path_view<Char>::extension() const
{
	using namespace detail::path_impl;

	Char const* const beg = string_view_type::data();
	Char const* const end = beg + string_view_type::size();
	return basic_path_view(find_extension(find_leaf_name(beg, end), end), end);
}


template<typename Char>
constexpr bool basic_path_view<Char>::has_trailing_separators() const
{
	using namespace detail::path_impl;

	Char const* const beg = string_view_type::data();
	Char const* const end = beg + string_view_type::size();
	return find_trailing_separator(beg, end) != end;
}

template<typename Char>
constexpr basic_path_view<Char> basic_path_view<Char>::without_trailing_separators() const
{
	using namespace detail::path_impl;

	Char const* const beg = string_view_type::data();
	Char const* const end = beg + string_view_type::size();
	return basic_path_view(beg, find_trailing_separator(beg, end));
}


#if 0
template<typename Char>
constexpr basic_path_view<Char> basic_path_view<Char>::render_lexically_normal(Char* const buffer)
{

}

template<typename Char>
constexpr basic_path_view<Char> basic_path_view<Char>::render_lexically_relative(basic_path_view const base, Char* const buffer)
{
}

template<typename Char>
constexpr basic_path_view<Char> basic_path_view<Char>::render_lexically_proximate(basic_path_view const base, Char* const buffer)
{
}
#endif

#if 0
template<typename Char>
constexpr bool lexically_equivalent(basic_path_view<Char> const lhs, basic_path_view<Char> const rhs)
{
	using namespace detail::path_impl;

	using path_view_type = basic_path_view<Char>;
	using string_view_type = std::basic_string_view<Char>;

	string_view_type const lhs_string = lhs.string();
	string_view_type const rhs_string = rhs.string();

	Char const* l_beg = lhs_string.data();
	Char const* const l_end = l_beg + lhs_string.size();

	Char const* r_beg = rhs_string.data();
	Char const* const r_end = r_beg + rhs_string.size();

	Char const* const l_root_name_end = find_root_name_end(l_beg, l_end);
	Char const* const r_root_name_end = find_root_name_end(r_beg, r_end);

	// If the root names are not equivalent, the paths are not equivalent.
	if (!std::equal(l_beg, l_root_name_end, r_beg, r_root_name_end,
		[](Char const a, Char const b) { return a == b || (is_separator(a) && is_separator(b)); }))
	{
		return false;
	}

	// Skip root directories.
	l_beg = skip_separators(l_root_name_end, l_end);
	r_beg = skip_separators(r_root_name_end, r_end);

	bool const absolute = l_root_name_end != l_beg;

	// Equivalent paths are either both absolute or both relative.
	if ((r_root_name_end != r_beg) != absolute)
	{
		return false;
	}

	size_t lhs_parent_count = 0;
	size_t rhs_parent_count = 0;

	size_t lhs_component_count = 0;
	size_t rhs_component_count = 0;

	size_t common_count = 0;

	while (true)
	{

	}
}
#endif

#if 0
template<typename Char>
constexpr bool basic_path_view<Char>::lexically_equivalent(basic_path_view<Char> const lhs, basic_path_view<Char> const rhs)
{
	using namespace detail::path_impl;

	using path_view_type = basic_path_view<Char>;
	using string_view_type = std::basic_string_view<Char>;

	string_view_type const lhs_string = lhs.string();
	string_view_type const rhs_string = rhs.string();

	Char const* l_beg = lhs_string.data();
	Char const* const l_end = l_beg + lhs_string.size();

	Char const* r_beg = rhs_string.data();
	Char const* const r_end = r_beg + rhs_string.size();

	Char const* const l_root_name_end = find_root_name_end(l_beg, l_end);
	Char const* const r_root_name_end = find_root_name_end(r_beg, r_end);

	// If the root names are not equivalent, the paths are not equivalent.
	if (!std::equal(l_beg, l_root_name_end, r_beg, r_root_name_end,
		[](Char a, Char b) { return a == b || (is_separator(a) && is_separator(b)); }))
	{
		return false;
	}

	// Skip root directories.
	l_beg = skip_separators(l_root_name_end, l_end);
	r_beg = skip_separators(r_root_name_end, r_end);

	// Equivalent paths are either both absolute or both relative.
	bool const absolute = l_root_name_end != l_beg;
	if ((r_root_name_end != r_beg) != absolute)
	{
		return false;
	}

	// Normalize the components of the relative paths of both paths.
	Components<Char> const components1 = NormalizeComponents(l_beg, l_end, absolute);
	Components<Char> const components2 = NormalizeComponents(l_beg, l_end, absolute);

	// The equivalence of two relative paths is determined by the equivalence of their components.
	return std::equal(components1.begin(), components1.end(), components2.begin(), components2.end());
}
#endif


template <typename Char>
constexpr bool basic_path_view<Char>::equal(basic_path_view const lhs, basic_path_view const rhs)
{
	using namespace detail::path_impl;

	string_view_type const lhs_string = lhs.string();
	string_view_type const rhs_string = rhs.string();

	Char const* l_beg = lhs_string.data();
	Char const* const l_end = l_beg + lhs_string.size();

	Char const* r_beg = rhs_string.data();
	Char const* const r_end = r_beg + lhs_string.size();

	if (root_path_compare(l_beg, l_end, r_beg, r_end).first)
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

template <typename Char>
constexpr int basic_path_view<Char>::compare(basic_path_view const lhs, basic_path_view const rhs)
{
	using namespace detail::path_impl;

	string_view_type const lhs_string = lhs.string();
	string_view_type const rhs_string = rhs.string();

	Char const* l_beg = lhs_string.data();
	Char const* const l_end = l_beg + lhs_string.size();

	Char const* r_beg = rhs_string.data();
	Char const* const r_end = r_beg + lhs_string.size();

	if (int const cmp = root_path_compare(l_beg, l_end, r_beg, r_end).first)
	{
		return cmp;
	}

	while (true)
	{
		Char const* const l_c_beg = l_beg;
		Char const* const r_c_beg = r_beg;

		l_beg = find_separator(l_beg, l_end);
		r_beg = find_separator(r_beg, r_end);

		// Compare components.
		if (int const cmp = compare_chars(l_c_beg, l_beg, r_c_beg, r_beg))
		{
			return cmp;
		}

		// If one path ends and the other doesn't, the comparison result depends on which path ends.
		if (bool const l_e = l_beg == l_end, r_e = r_beg == r_end; l_e || r_e)
		{
			return static_cast<int>(!l_e) - static_cast<int>(!r_e);
		}

		l_beg = skip_separators(l_beg + 1, l_end);
		r_beg = skip_separators(r_beg + 1, r_end);

		// If one path ends and the other doesn't, the comparison result depends on which path ends.
		if (bool const l_e = l_beg == l_end, r_e = r_beg == r_end; l_e || r_e)
		{
			return static_cast<int>(!l_e) - static_cast<int>(!r_e);
		}
	}
}


template<typename Char>
constexpr void basic_path_view<Char>::iterator::init_begin(Char const* const beg, Char const* const end)
{
	using namespace detail::path_impl;

	m_beg = beg;
	m_end = end;

	if (Char const* const root_name_end = find_root_name_end(beg, end); root_name_end != beg)
	{
		m_send = root_name_end;
	}
	else
	{
		if (Char const* const root_directory_end = skip_separators(beg, end); root_directory_end != beg)
		{
			m_send = root_directory_end;
		}
		else
		{
			m_send = find_separator(beg, end);
		}
	}
}

template<typename Char>
constexpr void basic_path_view<Char>::iterator::increment()
{
	using namespace detail::path_impl;

	// Let N+0 denote the component currently referred to by this iterator.
	// Let N+1 denote the next component which is the logical result of this function.

	Char const* const end = m_end;
	Char const* sbeg = m_sbeg;

	// If N+0 refers to the first component.
	if (sbeg == m_beg)
	{
		// If N+0 is the root name.
		if (Char const* const root_name_end = find_root_name_end(sbeg, end); root_name_end != sbeg)
		{
			// If the path also has a root directory, N+1 is the root directory.
			if (Char const* const root_path_end = skip_separators(root_name_end, end); root_name_end != root_path_end)
			{
				m_sbeg = root_name_end;
				m_send = root_path_end;
				return;
			}
		}
	}
	else if (is_separator(*sbeg) && sbeg == m_send)
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

template<typename Char>
constexpr void basic_path_view<Char>::iterator::decrement()
{
	using namespace detail::path_impl;

	// Let N+0 denote the component currently referred to by this iterator.
	// Let N+1 denote the next component which is the logical result of this function.

	Char const* const beg = m_beg;
	Char const* const end = m_end;

	Char const* const root_name_end = find_root_name_end(beg, end);
	Char const* const root_path_end = skip_separators(root_name_end, end);

	Char const* sbeg = m_sbeg;

	// N-1 is the root directory.
	if (root_name_end != root_path_end && sbeg == root_path_end)
	{
		m_sbeg = root_name_end;
		m_send = root_path_end;
		return;
	}

	// N-1 is the root name.
	if (beg != root_name_end && sbeg == root_name_end)
	{
		m_sbeg = beg;
		m_send = root_name_end;
		return;
	}

	// N-1 is the magic empty path at the end.
	if (sbeg == end && is_separator(sbeg[-1]))
	{
		m_sbeg = --sbeg;
		m_send = sbeg;
		return;
	}

	// Skip separators.
	while (sbeg != root_path_end && is_separator(sbeg[-1]))
	{
		--sbeg;
	}

	// Save N-1 end pointer.
	Char const* const send = sbeg;

	// Skip until next separator.
	while (sbeg != root_path_end && !is_separator(sbeg[-1]))
	{
		--sbeg;
	}

	m_sbeg = sbeg;
	m_send = send;
}


template<typename Char>
constexpr basic_path_combine_result<Char> combine_path(basic_path_view<Char> const lhs, basic_path_view<Char> const rhs)
{
	using namespace detail::path_impl;

	using path_view_type = basic_path_view<Char>;
	using string_view_type = std::basic_string_view<Char>;
	using result_type = basic_path_combine_result<Char>;

	string_view_type const l_string = lhs.string();
	Char const* const l_beg = l_string.data();
	Char const* const l_end = l_beg + l_string.size();

	if (l_beg == l_end)
	{
		return result_type(rhs);
	}

	string_view_type const r_string = rhs.string();
	Char const* const r_beg = r_string.data();
	Char const* const r_end = r_beg + r_string.size();

	Char const* const l_root_name_end = find_root_name_end(l_beg, l_end);

	// An added separator may be required if lhs does not end in a separator,
	// except in the case that lhs contains only a drive letter.
	bool const requires_separator = !path_view_type::is_separator(l_end[-1]) &&
		!(l_root_name_end == l_end && has_drive_letter(l_beg, l_root_name_end));

	// If rhs is empty, it is appended as an empty relative component to lhs.
	if (r_beg == r_end)
	{
		return result_type(lhs, requires_separator);
	}

	Char const* const r_root_name_end = find_root_name_end(r_beg, r_end);

	// If rhs has a root name and it is not the same drive as lhs, the result is rhs.
	if (r_beg != r_root_name_end && !is_same_drive(l_beg, l_end, r_beg, r_end))
	{
		return result_type(rhs);
	}

	// If rhs has a root directory, the result is rhs appended to the root name of lhs.
	if (r_root_name_end != r_end && path_view_type::is_separator(*r_root_name_end))
	{
		return result_type(
			path_view_type(l_beg, l_root_name_end),
			path_view_type(r_root_name_end, r_end));
	}

	// Otherwise the result is both paths combined, ignoring the root name of rhs,
	// with a separator in between if lhs does not end with a separator.
	return result_type(lhs, path_view_type(r_root_name_end, r_end), requires_separator);
}


using platform_path_view = basic_path_view<wchar_t>;
using platform_path_combine_result = basic_path_combine_result<wchar_t>;

} // namespace allio
