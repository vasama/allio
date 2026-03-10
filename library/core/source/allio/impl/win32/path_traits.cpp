#include <allio/win32/path_traits.hpp>

#include <allio/impl/win32/path_traits_impl.hpp>

#include <vsm/arrow.hpp>

#include <algorithm>

using namespace allio;
using namespace detail;

namespace {

using namespace detail::win32_path_traits_impl;

template<typename Char>
struct ptr_pair
{
	Char* beg;
	Char* end;
};

template<typename Char>
Char const* find_last(Char const* const beg, Char const* const end, Char const value)
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
Char const* skip_one_separator(Char const* const beg, Char const* const end)
{
	return beg + (beg != end && is_separator(*beg));
}

template<typename Char>
std::strong_ordering compare_chars(
	Char const* l_beg,
	Char const* const l_end,
	Char const* r_beg,
	Char const* const r_end)
{
	return std::lexicographical_compare_three_way(l_beg, l_end, r_beg, r_end);
}

template<typename Char>
bool is_drive_letter(Char const letter)
{
	return
		(static_cast<Char>('a') <= letter && letter <= static_cast<Char>('z')) ||
		(static_cast<Char>('A') <= letter && letter <= static_cast<Char>('Z'));
}

template<typename Char>
Char const* find_root_name_end(Char const* const beg, Char const* const end)
{
	// No root name is fewer than 2 characters in length.
	if (end - beg < 2)
	{
		return beg;
	}

	// Drive letter paths X: are most common.
	if (has_drive_letter(beg, end))
	{
		return beg + 2;
	}

	// All other root names begin with a separator and are no fewer than 3 characters in length.
	if (end - beg < 3 || !is_separator(beg[0]))
	{
		return beg;
	}

	// \??
	if (beg[1] == '?' && beg[2] == '?' && (end - beg == 3 || is_separator(beg[3])))
	{
		return beg + 3;
	}

	// \\? \\. \\server
	if (is_separator(beg[1]) && !is_separator(beg[2]))
	{
		return find_separator(beg + 3, end);
	}

	return beg;
}

template<typename Char>
Char const* find_root_path_end(Char const* const beg, Char const* const end)
{
	return skip_separators(find_root_name_end(beg, end), end);
}

template<typename Char>
Char const* find_leaf_name(Char const* const beg, Char const* end)
{
	Char const* const root_path_end = find_root_path_end(beg, end);
	while (root_path_end != end && !is_separator(end[-1]))
	{
		--end;
	}
	return end;
}

template<typename Char>
Char const* find_extension(Char const* const beg, Char const* const end)
{
	Char const* const ext = find_last(beg, end, static_cast<Char>('.'));

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
Char const* find_trailing_separator(Char const* const beg, Char const* end)
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
bool is_same_drive(
	Char const* const l_beg, Char const* const l_end,
	Char const* const r_beg, Char const* const r_end)
{
	return
		has_drive_letter(l_beg, l_end) &&
		has_drive_letter(r_beg, r_end) &&
		*l_beg == *r_beg;
}

template<typename Char>
std::strong_ordering compare_root_path(
	Char const*& l_beg_ref, Char const* const l_end,
	Char const*& r_beg_ref, Char const* const r_end)
{
	Char const* l_beg = l_beg_ref;
	Char const* r_beg = r_beg_ref;

	Char const* const l_root_name_end = find_root_name_end(l_beg, l_end);
	Char const* const r_root_name_end = find_root_name_end(r_beg, r_end);

	auto const root_name_order = compare_chars(l_beg, l_root_name_end, r_beg, r_root_name_end);

	// If the root names are not equal, the paths are not equivalent.
	if (root_name_order != 0)
	{
		return root_name_order;
	}

	// Skip root directories.
	l_beg = skip_separators(l_root_name_end, l_end);
	r_beg = skip_separators(r_root_name_end, r_end);

	bool const l_absolute = l_root_name_end != l_beg;
	bool const r_absolute = r_root_name_end != r_beg;

	// Equivalent paths are either both absolute or both relative.
	if (l_absolute != r_absolute)
	{
		return l_absolute <=> r_absolute;
	}

	l_beg_ref = l_beg;
	r_beg_ref = r_beg;

	return std::strong_ordering::equal;
}

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
		return ptr_pair<Char const>(m_pos, m_end);
	}

	[[nodiscard]] vsm::arrow<ptr_pair<Char const>> operator->() const
	{
		vsm_assert(m_pos != m_end);
		return ptr_pair<Char const>(m_pos, m_end);
	}

	normalizing_reverse_iterator& operator++() &
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
class reverse_path_builder
{
	string_buffer<Char> m_buffer;

	Char* m_beg = nullptr;
	Char* m_pos = nullptr;
	Char* m_end = nullptr;

	size_t m_max_path_size;

public:
	explicit reverse_path_builder(
		string_buffer<Char> const buffer,
		size_t const max_path_size = static_cast<size_t>(-1))
		: m_buffer(buffer)
		, m_max_path_size(max_path_size)
	{
	}

	[[nodiscard]] bool empty() const
	{
		return m_beg == m_pos;
	}

	[[nodiscard]] vsm::result<void> push_separator()
	{
		return vsm::discard_value(push_component(
			nullptr,
			nullptr,
			/* insert_trailing_separator: */ true));
	}

	[[nodiscard]] vsm::result<ptr_pair<Char>> push_component(
		Char const* const beg,
		Char const* const end,
		bool const insert_trailing_separator)
	{
		size_t const size = static_cast<size_t>(end - beg);
		vsm_try(out, push_uninitialized(size + insert_trailing_separator));

		if (insert_trailing_separator)
		{
			*out++ = '\\';
		}

		Char* const out_beg = out;
		Char* const out_end = std::reverse_copy(beg, end, out);

		return ptr_pair<Char>(out_beg, out_end);
	}

	[[nodiscard]] vsm::result<void> push_backtracking(
		size_t const backtrack_count,
		bool const insert_trailing_separator)
	{
		if (backtrack_count != 0)
		{
			size_t const backtrack_1 = !insert_trailing_separator;
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
		}

		return {};
	}

	[[nodiscard]] vsm::result<ptr_pair<Char>> finalize()
	{
		std::reverse(m_beg, m_pos);

		if (m_pos != m_end)
		{
			size_t const size = static_cast<size_t>(m_pos - m_beg);
			vsm_try_void(resize_buffer(size, size));
		}

		return ptr_pair<Char>(m_beg, m_pos);
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
		size_t const size = static_cast<size_t>(m_pos - m_beg);

		m_beg = new_buffer.data();
		m_pos = new_buffer.data() + size;
		m_end = new_buffer.data() + new_buffer.size();

		return {};
	}
};

} // namespace

template<typename Char>
bool win32_path_traits_base<Char>::is_absolute(string_view<Char> const string)
{
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
string_view<Char> win32_path_traits_base<Char>::root_name(string_view<Char> const string)
{
	Char const* const beg = string.data();
	return string_view<Char>(
		beg,
		find_root_name_end(beg, beg + string.size()));
}

template<typename Char>
string_view<Char> win32_path_traits_base<Char>::root_directory(string_view<Char> const string)
{
	Char const* const beg = string.data();
	Char const* const end = beg + string.size();
	Char const* const root_name_end = find_root_name_end(beg, end);
	Char const* const root_directory_end = skip_one_separator(root_name_end, end);
	return string_view<Char>(root_name_end, root_directory_end);
}

template<typename Char>
string_view<Char> win32_path_traits_base<Char>::root_path(string_view<Char> const string)
{
	Char const* const beg = string.data();
	Char const* const end = beg + string.size();
	Char const* const root_name_end = find_root_name_end(beg, end);
	Char const* const root_directory_end = skip_one_separator(root_name_end, end);
	return string_view<Char>(beg, root_directory_end);
}

template<typename Char>
string_view<Char> win32_path_traits_base<Char>::relative_path(string_view<Char> const string)
{
	Char const* const beg = string.data();
	Char const* const end = beg + string.size();
	Char const* const root_end = find_root_path_end(beg, end);
	return string_view<Char>(root_end, end);
}

template<typename Char>
string_view<Char> win32_path_traits_base<Char>::parent_path(string_view<Char> const string)
{
	Char const* const beg = string.data();
	Char const* end = beg + string.size();

	Char const* const root_name_end = find_root_name_end(beg, end);
	Char const* const root_path_end = skip_separators(root_name_end, end);

	while (root_path_end != end && !is_separator(end[-1]))
	{
		--end;
	}

	while (root_path_end != end && is_separator(end[-1]))
	{
		--end;
	}

	if (root_path_end == end)
	{
		end = skip_one_separator(root_name_end, end);
	}

	return string_view<Char>(beg, end);
}

template<typename Char>
string_view<Char> win32_path_traits_base<Char>::filename(string_view<Char> const string)
{
	Char const* const beg = string.data();
	Char const* const end = beg + string.size();
	return string_view<Char>(find_leaf_name(beg, end), end);
}

template<typename Char>
string_view<Char> win32_path_traits_base<Char>::stem(string_view<Char> const string)
{
	Char const* const beg = string.data();
	Char const* const end = beg + string.size();
	Char const* const leaf = find_leaf_name(beg, end);
	return string_view<Char>(leaf, find_extension(leaf, end));
}

template<typename Char>
string_view<Char> win32_path_traits_base<Char>::extension(string_view<Char> const string)
{
	Char const* const beg = string.data();
	Char const* const end = beg + string.size();
	return string_view<Char>(find_extension(find_leaf_name(beg, end), end), end);
}

template<typename Char>
bool win32_path_traits_base<Char>::has_trailing_separators(string_view<Char> const string)
{
	Char const* const beg = string.data();
	Char const* const end = beg + string.size();
	return find_trailing_separator(beg, end) != end;
}

template<typename Char>
string_view<Char> win32_path_traits_base<Char>::without_trailing_separators(
	string_view<Char> const string)
{
	Char const* const beg = string.data();
	Char const* const end = beg + string.size();
	return string_view<Char>(beg, find_trailing_separator(beg, end));
}

template<typename Char>
bool win32_path_traits_base<Char>::equal(string_view<Char> const lhs, string_view<Char> const rhs)
{
	Char const* l_beg = lhs.data();
	Char const* const l_end = l_beg + lhs.size();

	Char const* r_beg = rhs.data();
	Char const* const r_end = r_beg + rhs.size();

	if (compare_root_path(l_beg, l_end, r_beg, r_end) != 0)
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
std::strong_ordering win32_path_traits_base<Char>::compare(
	string_view<Char> const lhs,
	string_view<Char> const rhs)
{
	Char const* l_beg = lhs.data();
	Char const* const l_end = l_beg + lhs.size();

	Char const* r_beg = rhs.data();
	Char const* const r_end = r_beg + rhs.size();

	auto const root_path_order = compare_root_path(l_beg, l_end, r_beg, r_end);

	if (root_path_order != 0)
	{
		return root_path_order;
	}

	while (true)
	{
		Char const* const l_c_beg = l_beg;
		Char const* const r_c_beg = r_beg;

		l_beg = find_separator(l_beg, l_end);
		r_beg = find_separator(r_beg, r_end);

		auto const component_order = compare_chars(l_c_beg, l_beg, r_c_beg, r_beg);

		// Compare components.
		if (component_order != 0)
		{
			return component_order;
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

template<typename Char>
bool win32_path_traits_base<Char>::lexically_equivalent(
	string_view<Char> const lhs,
	string_view<Char> const rhs)
{
	Char const* const l_beg = lhs.data();
	Char const* const l_end = l_beg + lhs.size();

	Char const* const r_beg = rhs.data();
	Char const* const r_end = r_beg + rhs.size();

	Char const* const l_root_name_end = find_root_name_end(l_beg, l_end);
	Char const* const r_root_name_end = find_root_name_end(r_beg, r_end);

	auto const are_equivalent = [](Char const lhs, Char const rhs) -> bool
	{
		return lhs == rhs || (is_separator(lhs) && is_separator(rhs));
	};

	// If the root names are not equivalent, the paths are not equivalent.
	if (!std::equal(l_beg, l_root_name_end, r_beg, r_root_name_end, are_equivalent))
	{
		return false;
	}

	// Skip root directories.
	Char const* const l_root_end = skip_separators(l_root_name_end, l_end);
	Char const* const r_root_end = skip_separators(r_root_name_end, r_end);

	bool const absolute = l_root_name_end != l_root_end;

	// Equivalent paths are either both absolute or both relative.
	if (absolute != (r_root_name_end != r_root_end))
	{
		return false;
	}

	// If one path has relative components, and the other does not, the paths are not equivalent.
	// In any case, there is no need to continue if neither path has relative components.
	if (l_root_end == l_end)
	{
		return r_root_end == r_end;
	}

	Char const* const l_relative_end = skip_last_separators(l_root_end, l_end);
	Char const* const r_relative_end = skip_last_separators(r_root_end, r_end);

	normalizing_reverse_iterator<Char> l_it(l_root_end, l_relative_end);
	normalizing_reverse_iterator<Char> r_it(r_root_end, r_relative_end);

	// If one path has meaningful trailing separators and the other does not, the paths are not
	// equivalent.
	if (l_it && r_it)
	{
		bool const l_sep = l_relative_end != l_end || l_it->end != l_relative_end;
		bool const r_sep = r_relative_end != r_end || r_it->end != r_relative_end;

		if (l_sep != r_sep)
		{
			return false;
		}
	}

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

template<typename Char>
bool win32_path_traits_base<Char>::is_lexically_normal(string_view<Char> const string)
{
	Char const* const beg = string.data();
	Char const* const end = beg + string.size();

	// As a special case, a lone dot is considered lexically normal.
	if (end - beg == 1 && *beg == '.')
	{
		return true;
	}

	Char const* const root_name_end = find_root_name_end(beg, end);

	if (beg != root_name_end && is_separator(*beg))
	{
		// Root name containing forward slashes is not lexically normal.
		if (std::find(beg, root_name_end, '/') != root_name_end)
		{
			return false;
		}
	}

	Char const* const root_path_end = skip_separators(root_name_end, end);

	auto const is_normal_separator = [](Char const* const beg, Char const* const end)
	{
		return end - beg == 1 && *beg == '\\';
	};

	// A root directory separator not composed of a single backslash is not lexically normal.
	if (root_name_end != root_path_end && !is_normal_separator(root_name_end, root_path_end))
	{
		return false;
	}

	// Relative paths may contain backtracking at the beginning.
	bool may_contain_backtracking = root_name_end == root_path_end;

	Char const* pos = root_path_end;

	while (pos != end)
	{
		Char const* const sep_beg = find_separator(pos, end);
		Char const* const sep_end = skip_separators(sep_beg, end);

		// A separator composed of more than a single backslash is not lexically normal.
		if (sep_beg != sep_end && !is_normal_separator(sep_beg, sep_end))
		{
			return false;
		}

		/**/ if (*pos == '.' && sep_beg - pos == 1)
		{
			return false;
		}
		else if (*pos == '.' && sep_beg - pos == 2 && pos[1] == '.')
		{
			if (!may_contain_backtracking)
			{
				return false;
			}

			// A backtracking component followed by a separator at the end of the path is not
			// considered lexically normal.
			if (sep_beg != sep_end && sep_end == end)
			{
				return false;
			}
		}
		else
		{
			may_contain_backtracking = false;
		}

		pos = sep_end;
	}

	return true;
}

template<typename Char>
vsm::result<string_view<Char>> win32_path_traits_base<Char>::copy_lexically_normal(
	string_view<Char> const string,
	string_buffer<Char> const buffer)
{
	Char const* const beg = string.data();
	Char const* const end = beg + string.size();

	if (beg == end)
	{
		vsm_try(new_buffer, buffer.resize(0));
		return string_view<Char>(new_buffer);
	}

	Char const* const root_name_end = find_root_name_end(beg, end);
	Char const* const root_end = skip_separators(root_name_end, end);
	Char const* const relative_end = skip_last_separators(root_end, end);

	reverse_path_builder<Char> builder(
		buffer,
		/* max_path_size: */ static_cast<size_t>(end - beg));

	bool requires_separator = relative_end != end;

	normalizing_reverse_iterator<Char> iterator(root_end, relative_end);
	for (; iterator; ++iterator)
	{
		auto const [c_beg, c_end] = *iterator;
		vsm_try_discard(builder.push_component(
			c_beg,
			c_end,
			std::exchange(requires_separator, true) || c_end != end));
	}

	// Backtracking is only preserved in relative paths, including drive-relative paths.
	if (root_name_end == root_end)
	{
		if (size_t const backtrack_count = iterator.backtrack_count())
		{
			vsm_try_void(builder.push_backtracking(
				backtrack_count,
				requires_separator && !builder.empty()));
		}
	}

	// Finally the root name, if present, must be copied over.
	if (beg != root_name_end)
	{
		vsm_try_bind((out_root_beg, out_root_end), builder.push_component(
			beg,
			root_name_end,
			/* insert_trailing_separator: */ root_name_end != root_end));

		if (is_separator(*beg))
		{
			// The root name contains separators which must be normalized. Any such root name is no
			// less than two characters long, and only the first two characters can be separators.
			std::replace(out_root_end - 2, out_root_end, '/', '\\');
		}
	}
	else if (root_name_end != root_end)
	{
		vsm_try_void(builder.push_separator());
	}

	if (builder.empty())
	{
		vsm_try(new_buffer, buffer.resize(1));
		new_buffer.front() = static_cast<Char>('.');
		return string_view<Char>(new_buffer);
	}
	else
	{
		vsm_try_bind((out_beg, out_end), builder.finalize());
		return string_view<Char>(out_beg, out_end);
	}
}

template<typename Char>
path_combine_result_base<Char> win32_path_traits_base<Char>::combine(
	string_view<Char> const lhs,
	string_view<Char> const rhs)
{
	Char const* const l_beg = lhs.data();
	Char const* const l_end = l_beg + lhs.size();

	if (l_beg == l_end)
	{
		return path_combine_result_base<Char>(rhs);
	}

	Char const* const r_beg = rhs.data();
	Char const* const r_end = r_beg + rhs.size();

	Char const* const l_root_name_end = find_root_name_end(l_beg, l_end);

	// An added separator is required if lhs does not end in a separator, except in the case that
	// lhs contains only a drive letter.
	bool const requires_separator =
		!is_separator(l_end[-1]) &&
		!(l_root_name_end == l_end && has_drive_letter(l_beg, l_root_name_end));

	// If rhs is empty, it is appended as an empty relative component to lhs.
	if (r_beg == r_end)
	{
		return path_combine_result_base<Char>(lhs, string_view<Char>(), requires_separator);
	}

	Char const* const r_root_name_end = find_root_name_end(r_beg, r_end);

	// If rhs has a root name and it is not the same drive as lhs, the result is rhs.
	if (r_beg != r_root_name_end && !is_same_drive(l_beg, l_end, r_beg, r_end))
	{
		return path_combine_result_base<Char>(
			rhs,
			string_view<Char>(),
			/* requires_separator: */ false);
	}

	// If rhs has a root directory, the result is rhs appended to the root name of lhs.
	if (r_root_name_end != r_end && is_separator(*r_root_name_end))
	{
		return path_combine_result_base<Char>(
			string_view<Char>(l_beg, l_root_name_end),
			string_view<Char>(r_root_name_end, r_end),
			/* requires_separator: */ false);
	}

	// Otherwise the result is both paths combined, ignoring the root name of rhs, with a separator
	// in between if lhs does not end with a separator.
	return path_combine_result_base<Char>(
		lhs,
		string_view<Char>(r_root_name_end, r_end),
		requires_separator);
}

template<typename Char>
void win32_path_iterator_base<Char>::increment()
{
	// Let N+0 denote the component currently referred to by this iterator.
	// Let N+1 denote the next component which is the logical result of this function.

	vsm_assert(m_cur_beg != m_end);

	Char const* const beg = m_beg;
	Char const* const end = m_end;

	Char const* const cur_beg = m_cur_beg;
	Char const* cur_end = m_cur_end;

	if (beg == cur_beg)
	{
		Char const* const root_name_end = find_root_name_end(beg, end);

		// N+0 is the reverse end position.
		// N+1 is the root name.
		if (beg != root_name_end && cur_beg == cur_end)
		{
			m_cur_beg = beg;
			m_cur_end = root_name_end;
			return;
		}

		if (cur_end == root_name_end)
		{
			Char const* const root_directory_end = skip_one_separator(root_name_end, end);

			// N+1 is the root directory.
			if (root_name_end != root_directory_end)
			{
				m_cur_beg = root_name_end;
				m_cur_end = root_directory_end;
				return;
			}
		}
	}

	// N+0 is the magic empty path at the end.
	// N+1 is the forward end position.
	if (beg != cur_beg && cur_beg == cur_end)
	{
		vsm_assert(cur_end == end - 1);
		vsm_assert(is_separator(*cur_beg));
		m_cur_beg = end;
		m_cur_end = end;
		return;
	}

	// A one character separator component must be the root directory.
	// Skip redundant root directory separators before proceeding.
	if (cur_end - cur_beg == 1 && is_separator(*cur_beg))
	{
		cur_end = skip_separators(cur_end, end);
	}

	if (cur_end == end)
	{
		m_cur_beg = end;
		m_cur_end = end;
		return;
	}

	// There may be multiple separators.
	Char const* const new_beg = skip_separators(cur_end, end);

	// N+1 is the magic empty path at the end.
	if (new_beg == end)
	{
		m_cur_beg = end - 1;
		m_cur_end = end - 1;
		return;
	}

	// N+1 is the next regular path component.
	m_cur_beg = new_beg;
	m_cur_end = find_separator(new_beg, end);
}

template<typename Char>
void win32_path_iterator_base<Char>::decrement()
{
	// Let N+0 denote the component currently referred to by this iterator.
	// Let N-1 denote the previous component which is the logical result of this function.

	vsm_assert(m_beg != m_cur_beg || m_cur_beg != m_cur_end);

	Char const* const beg = m_beg;
	Char const* const end = m_end;

	Char const* const cur_beg = m_cur_beg;

	// N-1 is the reverse end position.
	if (beg == cur_beg)
	{
		m_cur_beg = beg;
		m_cur_end = beg;
		return;
	}

	Char const* const root_name_end = find_root_name_end(beg, end);
	Char const* const root_path_end = skip_separators(root_name_end, end);

	// N-1 is the root directory.
	if (cur_beg == root_path_end && root_name_end != root_path_end)
	{
		m_cur_beg = root_name_end;
		m_cur_end = root_name_end + 1;
		return;
	}

	// N-1 is the root name.
	if (beg != root_name_end && cur_beg == root_name_end)
	{
		m_cur_beg = beg;
		m_cur_end = root_name_end;
		return;
	}

	// N-1 is the magic empty path at the end.
	if (cur_beg == end && is_separator(end[-1]))
	{
		m_cur_beg = end - 1;
		m_cur_end = end - 1;
		return;
	}

	// N-1 is the previous regular component.
	Char const* const new_end = skip_last_separators(root_path_end, cur_beg);
	Char const* const new_beg = find_last_separator(root_path_end, new_end);

	m_cur_beg = new_beg == new_end ? root_path_end : new_beg + 1;
	m_cur_end = new_end;
}

template struct win32_path_iterator_base<char>;
template struct win32_path_iterator_base<wchar_t>;
template struct win32_path_iterator_base<char8_t>;
template struct win32_path_iterator_base<char16_t>;
template struct win32_path_iterator_base<char32_t>;

template struct win32_path_traits_base<char>;
template struct win32_path_traits_base<wchar_t>;
template struct win32_path_traits_base<char8_t>;
template struct win32_path_traits_base<char16_t>;
template struct win32_path_traits_base<char32_t>;
