#pragma once

#include <allio/any_path_buffer.hpp>
#include <allio/detail/path.hpp>

#include <vsm/assert.h>

#include <algorithm>
#include <compare>

namespace allio {
namespace detail::path_impl {

template<typename Char>
using ptr_pair = std::pair<Char*>;

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
constexpr Char const* find_last_separator(Char const* const beg, Char const* const end)
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
constexpr Char const* skip_separators(Char const* const beg, Char const* const end)
{
	return std::find_if_not(beg, end, [](Char const character)
	{
		return is_separator(character);
	});
}

template<typename Char>
constexpr Char const* skip_last_separators(Char const* const beg, Char const* const end)
{
	Char const* pos = end;
	while (pos != beg && is_separator(pos[-1]))
	{
		--pos;
	}
	return end;
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

#if 0
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
#endif

template<typename Char>
class normalizing_reverse_iterator
{
	Char const* m_beg;
	Char const* m_pos;
	Char const* m_end;

	size_t m_backtrack_count = 0;

public:
	explicit normalizing_reverse_iterator(Char const* const beg, Char const* const end)
		: m_beg(beg)
		, m_pos(end)
		, m_end(end)
	{
		vsm_assert(beg == end || !is_separator(end[-1]));

		if (beg != end)
		{
			find_next_component();
		}
	}

	[[nodiscard]] ptr_pair<Char const> operator*() const
	{
		vsm_assert(m_pos != m_end);
		return { m_pos, m_end };
	}

	[[nodiscard]] normalizing_reverse_iterator& operator++() &
	{
		vsm_assert(m_pos != m_end);
		find_next_component();
		return *this;
	}

	[[nodiscard]] explicit operator bool() const
	{
		return m_pos != m_end;
	}

	[[nodiscard]] size_t backtrack_count() const
	{
		return m_backtrack_count;
	}

private:
	void find_next_component()
	{
		while (true)
		{
			m_end = skip_last_separators(m_beg, m_pos);
			m_pos = m_end;

			if (m_beg == m_end)
			{
				break;
			}

			Char const* const sep = find_last_separator(m_beg, m_end);
			m_pos = sep == m_end ? m_beg : sep + 1;

			if (m_pos[0] == '.')
			{
				if (m_end - m_pos == 1)
				{
					continue;
				}

				if (m_end - m_pos == 2 && m_pos[1] == '.')
				{
					++m_backtrack_count;
					continue;
				}
			}

			if (m_backtrack_count != 0)
			{
				--m_backtrack_count;
			}
			else
			{
				break;
			}
		}
	}
};

template<typename Char>
class reversed_path_builder
{
	string_buffer<Char> m_buffer;

	Char* m_beg = nullptr;
	Char* m_pos = nullptr;
	Char* m_end = nullptr;

	bool m_requires_separator;
	size_t m_max_path_size;

public:
	explicit reversed_path_builder(
		string_buffer<Char> const buffer,
		bool const trailing_separator,
		size_t const max_path_size = static_cast<size_t>(-1))
		: m_buffer(buffer)
		, m_requires_separator(trailing_separator)
		, m_max_path_size(max_path_size)
	{
	}

	[[nodiscard]] vsm::result<ptr_pair<Char>> push_component(
		Char const* const beg,
		Char const* const end)
	{
		vsm_try(out, push_uninitialized(static_cast<size_t>(end - beg) + m_requires_separator));

		if (m_requires_separator)
		{
			*out++ = '\\';
		}
		m_requires_separator = true;

		Char* const out_beg = out;
		Char* const out_end = std::reverse_copy(beg, end, out);

		return ptr_pair<Char>(out_beg, out_end);
	}

	[[nodiscard]] vsm::result<void> push_empty_component()
	{
		return vsm::discard_value(push_component(nullptr, nullptr));
	}

	[[nodiscard]] vsm::result<void> push_backtracking(size_t const backtrack_count)
	{
		if (backtrack_count != 0)
		{
			size_t const backtrack_1 = backtrack_count - !m_requires_separator;
			size_t const backtrack_n = backtrack_count - backtrack_1;
			vsm_try(out, push_uninitialized(backtrack_n * 3 + backtrack_1 * 2));

			if (backtrack_1 != 0)
			{
				*out++ = '.';
				*out++ = '.';
			}

			for (size_t i = 0; i < backtrack_n; ++i)
			{
				*out++ = '\\';
				*out++ = '.';
				*out++ = '.';
			}

			m_requires_separator = true;
		}

		return {};
	}

	[[nodiscard]] vsm::result<ptr_pair<Char>> finalize()
	{
		if (m_pos != m_end)
		{
			size_t const size = static_cast<size_t>(m_pos - m_beg);
			vsm_try_void(resize_buffer(size, size));
		}

		return std::pair<Char*, Char*>(m_beg, m_pos);
	}

private:
	[[nodiscard]] vsm::result<Char*> push_uninitialized(size_t const size)
	{
		size_t const cur_size = static_cast<size_t>(m_pos - m_beg);
		size_t const min_size = cur_size + size;
		vsm_assert(min_size <= m_max_path_size);

		vsm_try_void(resize_buffer(min_size, m_max_path_size));

		Char* const pos = m_pos;
		m_pos = pos + size;
		return pos;
	}

	[[nodiscard]] vsm::result<void> resize_buffer(size_t const min_size, size_t const max_size)
	{
		vsm_try(new_buffer, m_buffer.resize(min_size, max_size));

		m_beg = new_buffer.data();
		m_pos = new_buffer.data() + static_cast<size_t>(m_pos - m_beg)
		m_end = new_buffer.data() + new_buffer.size();

		return {};
	}
};

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

} // namespace detail::path_impl


template<typename Char, typename Encoding>
constexpr bool basic_path_view<Char, Encoding>::is_absolute() const
{
	using namespace detail::path_impl;

	Char const* const beg = m_string_view.data();
	Char const* const end = beg + m_string_view.size();

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


template<typename Char, typename Encoding>
constexpr basic_path_view<Char, Encoding> basic_path_view<Char, Encoding>::root_name() const
{
	using namespace detail::path_impl;

	Char const* const beg = m_string_view.data();
	return basic_path_view(string_view_type(
		beg,
		find_root_name_end(beg, beg + m_string_view.size())));
}

template<typename Char, typename Encoding>
constexpr basic_path_view<Char, Encoding> basic_path_view<Char, Encoding>::root_directory() const
{
	using namespace detail::path_impl;

	Char const* const beg = m_string_view.data();
	Char const* const end = beg + m_string_view.size();
	Char const* const root_name_end = find_root_name_end(beg, end);
	return basic_path_view(root_name_end, skip_separators(root_name_end, end));
}

template<typename Char, typename Encoding>
constexpr basic_path_view<Char, Encoding> basic_path_view<Char, Encoding>::root_path() const
{
	using namespace detail::path_impl;

	Char const* const beg = m_string_view.data();
	Char const* const end = beg + m_string_view.size();
	return basic_path_view(beg, find_root_path_end(beg, end));
}

template<typename Char, typename Encoding>
constexpr basic_path_view<Char, Encoding> basic_path_view<Char, Encoding>::relative_path() const
{
	using namespace detail::path_impl;

	Char const* const beg = m_string_view.data();
	Char const* const end = beg + m_string_view.size();
	return basic_path_view(find_root_path_end(beg, end), end);
}

template<typename Char, typename Encoding>
constexpr basic_path_view<Char, Encoding> basic_path_view<Char, Encoding>::parent_path() const
{
	using namespace detail::path_impl;

	Char const* const beg = m_string_view.data();
	Char const* end = beg + m_string_view.size();
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

template<typename Char, typename Encoding>
constexpr basic_path_view<Char, Encoding> basic_path_view<Char, Encoding>::filename() const
{
	using namespace detail::path_impl;

	Char const* const beg = m_string_view.data();
	Char const* const end = beg + m_string_view.size();
	return basic_path_view(find_leaf_name(beg, end), end);
}

template<typename Char, typename Encoding>
constexpr basic_path_view<Char, Encoding> basic_path_view<Char, Encoding>::stem() const
{
	using namespace detail::path_impl;

	Char const* const beg = m_string_view.data();
	Char const* const end = beg + m_string_view.size();
	Char const* const leaf = find_leaf_name(beg, end);
	return basic_path_view(leaf, find_extension(leaf, end));
}

template<typename Char, typename Encoding>
constexpr basic_path_view<Char, Encoding> basic_path_view<Char, Encoding>::extension() const
{
	using namespace detail::path_impl;

	Char const* const beg = m_string_view.data();
	Char const* const end = beg + m_string_view.size();
	return basic_path_view(find_extension(find_leaf_name(beg, end), end), end);
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
	return basic_path_view(beg, find_trailing_separator(beg, end));
}


template<typename Char, typename Encoding>
constexpr vsm::result<basic_path_view<Char, Encoding>> basic_path_view<Char, Encoding>::render_lexically_normal(
	path_buffer<Char> const buffer)
{
	using namespace detail::path_impl;

	Char const* const beg = m_string_view.data();
	Char const* const end = beg + m_string_view.size();

	Char const* const root_name_end = find_root_name_end(beg, end);
	Char const* const relative_beg = skip_separators(root_name_end, end);
	Char const* const relative_end = skip_last_separators(relative_beg, end);

	reversed_path_builder<Char> builder(
		buffer,
		/* trailing_separator: */ relative_end != end,
		/* max_path_size: */ static_cast<size_t>(end - beg));

	normalizing_reverse_iterator<Char> iterator(relative_beg, relative_end);
	for (; iterator; ++iterator)
	{
		auto const [c_beg, c_end] = *iterator;
		vsm_try_void(builder.push_component(c_beg, c_end));
	}

	// Backtracking is only preserved in relative paths, including drive-relative paths.
	if (root_name_end == relative_beg)
	{
		vsm_try_void(builder.push_backtracking(iterator.backtrack_count()));
	}

	// Finally the root name, if present, must be copied over. Otherwise the root separator, if
	// present, is reintroduced by pushing an empty component.
	/**/ if (beg != root_name_end)
	{
		vsm_try_bind((out_root_beg, out_root_end), push_component(beg, root_name_end));

		if (is_separator(*beg))
		{
			// The root name contains separators which must be normalized. Any such root name is no
			// less than two characters long, and only the first two characters can be separators.
			std::replace(out_root_end - 2, out_root_end, '/', '\\');
		}
	}
	else if (root_name_end != relative_beg)
	{
		vsm_try_void(builder.push_empty_component());
	}

	vsm_try_bind((out_beg, out_end), builder.finalize());
	return basic_path_view<Char, Encoding>(out_beg, out_pos);
}

#if 0
template<typename Char, typename Encoding>
constexpr basic_path_view<Char, Encoding> basic_path_view<Char, Encoding>::render_lexically_relative(basic_path_view const base, Char* const buffer)
{
}

template<typename Char, typename Encoding>
constexpr basic_path_view<Char, Encoding> basic_path_view<Char, Encoding>::render_lexically_proximate(basic_path_view const base, Char* const buffer)
{
}
#endif

#if 1
template<typename Char, typename Encoding>
constexpr bool basic_path_view<Char, Encoding>::lexically_equivalent(
	basic_path_view<Char, Encoding> const lhs,
	basic_path_view<Char, Encoding> const rhs)
{
	using namespace detail::path_impl;

	using path_view_type = basic_path_view<Char, Encoding>;
	using string_view_type = std::basic_string_view<Char>;

	string_view_type const lhs_string = lhs.string();
	string_view_type const rhs_string = rhs.string();

	Char const* l_beg = lhs_string.data();
	Char const* l_end = l_beg + lhs_string.size();

	Char const* r_beg = rhs_string.data();
	Char const* r_end = r_beg + rhs_string.size();

	Char const* const l_root_name_end = find_root_name_end(l_beg, l_end);
	Char const* const r_root_name_end = find_root_name_end(r_beg, r_end);

	auto const are_equivalent = [](Char const lhs, Char const rhs) -> bool
	{
		return a == b || (is_separator(a) && is_separator(b));
	};

	// If the root names are not equivalent, the paths are not equivalent.
	if (!std::equal(l_beg, l_root_name_end, r_beg, r_root_name_end, are_equivalent))
	{
		return false;
	}

	// Skip root directories.
	l_beg = skip_separators(l_root_name_end, l_end);
	r_beg = skip_separators(r_root_name_end, r_end);

	bool const absolute = l_root_name_end != l_beg;

	// Equivalent paths are either both absolute or both relative.
	if (absolute != (r_root_name_end != r_beg))
	{
		return false;
	}

	normalizing_reverse_iterator<Char> l_it(l_beg, l_end);
	normalizing_reverse_iterator<Char> r_it(l_beg, l_end);

	for (; l_it && r_it; ++l_it, ++r_it)
	{
		auto const [l_c_beg, l_c_end] = *l_it;
		auto const [r_c_beg, r_c_end] = *r_it;

		if (!std::equal(l_c_beg, l_c_end, r_c_beg, r_c_end))
		{
			return false;
		}
	}

	// Equivalent relative paths involve equal amounts of backtracking.
	return !l_it && !r_it && (absolute || l_it.backtrack_count() == r_it.backtrack_count());
}
#endif

#if 0
template<typename Char, typename Encoding>
constexpr bool basic_path_view<Char, Encoding>::lexically_equivalent(basic_path_view<Char, Encoding> const lhs, basic_path_view<Char, Encoding> const rhs)
{
	using namespace detail::path_impl;

	using path_view_type = basic_path_view<Char, Encoding>;
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


template<typename Char, typename Encoding>
constexpr bool basic_path_view<Char, Encoding>::equal(basic_path_view const lhs, basic_path_view const rhs)
{
	using namespace detail::path_impl;

	string_view_type const lhs_string = lhs.string();
	string_view_type const rhs_string = rhs.string();

	Char const* l_beg = lhs_string.data();
	Char const* const l_end = l_beg + lhs_string.size();

	Char const* r_beg = rhs_string.data();
	Char const* const r_end = r_beg + lhs_string.size();

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

template<typename Char, typename Encoding>
constexpr std::strong_ordering basic_path_view<Char, Encoding>::compare(
	basic_path_view const lhs,
	basic_path_view const rhs)
{
	using namespace detail::path_impl;

	string_view_type const lhs_string = lhs.string();
	string_view_type const rhs_string = rhs.string();

	Char const* l_beg = lhs_string.data();
	Char const* const l_end = l_beg + lhs_string.size();

	Char const* r_beg = rhs_string.data();
	Char const* const r_end = r_beg + lhs_string.size();

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


template<typename Char, typename Encoding>
constexpr void basic_path_view<Char, Encoding>::iterator::init_begin(
	Char const* const beg,
	Char const* const end)
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

template<typename Char, typename Encoding>
constexpr void basic_path_view<Char, Encoding>::iterator::increment()
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

template<typename Char, typename Encoding>
constexpr void basic_path_view<Char, Encoding>::iterator::decrement()
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


template<typename Char, typename Encoding>
constexpr basic_path_combine_result<Char, Encoding> allio::combine_path(
	basic_path_view<Char, Encoding> const lhs,
	basic_path_view<Char, Encoding> const rhs)
{
	using namespace detail::path_impl;

	using path_view_type = basic_path_view<Char, Encoding>;
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

} // namespace allio
