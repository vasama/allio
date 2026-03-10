#pragma once

#include <allio/any_string_buffer.hpp>
#include <allio/detail/path.hpp>

#include <vsm/assert.h>

#include <compare>
#include <string_view>

namespace allio {
namespace detail {

template<typename Char>
using string_view = std::basic_string_view<Char>;

template<typename Char>
struct posix_path_iterator_base
{
	Char const* m_beg;
	Char const* m_cur_beg;
	Char const* m_cur_end;
	Char const* m_end;

	void increment();
	void decrement();
};

template<typename Char>
class posix_path_iterator : posix_path_iterator_base<Char>
{
public:
	void set_reverse_end(Char const* const beg, Char const* const end)
	{
		this->m_beg = beg;
		this->m_cur_beg = beg;
		this->m_cur_end = beg;
		this->m_end = end;
	}

	void set_forward_end(Char const* const beg, Char const* const end)
	{
		this->m_beg = beg;
		this->m_cur_beg = end;
		this->m_cur_end = end;
		this->m_end = end;
	}

	string_view<Char> current() const
	{
		return string_view<Char>(this->m_cur_beg, this->m_cur_end);
	}

	using posix_path_iterator_base<Char>::increment;
	using posix_path_iterator_base<Char>::decrement;

	[[nodiscard]] friend bool operator==(posix_path_iterator const& lhs, posix_path_iterator const& rhs)
	{
		return lhs.m_cur_beg == rhs.m_cur_beg && lhs.m_cur_end == rhs.m_cur_end;
	}

	[[nodiscard]] friend bool operator!=(posix_path_iterator const& lhs, posix_path_iterator const& rhs)
	{
		return lhs.m_cur_beg != rhs.m_cur_beg || lhs.m_cur_end != rhs.m_cur_end;
	}
};

template<typename Char>
struct posix_path_traits_base
{
	static bool is_absolute(string_view<Char> string);
	static string_view<Char> root_name(string_view<Char> string);
	static string_view<Char> root_directory(string_view<Char> string);
	static string_view<Char> root_path(string_view<Char> string);
	static string_view<Char> relative_path(string_view<Char> string);
	static string_view<Char> parent_path(string_view<Char> string);
	static string_view<Char> filename(string_view<Char> string);
	static string_view<Char> stem(string_view<Char> string);
	static string_view<Char> extension(string_view<Char> string);

	static bool has_trailing_separators(string_view<Char> string);
	static string_view<Char> without_trailing_separators(string_view<Char> string);

	static bool equal(string_view<Char> lhs, string_view<Char> rhs);
	static std::strong_ordering compare(string_view<Char> lhs, string_view<Char> rhs);
	static bool lexically_equivalent(string_view<Char> lhs, string_view<Char> rhs);

	static bool is_lexically_normal(string_view<Char> string);

	static vsm::result<string_view<Char>> copy_lexically_normal(
		string_view<Char> string,
		string_buffer<Char> buffer);

#if 0
	static vsm::result<string_view<Char>> copy_lexically_relative(
		string_view<Char> string,
		string_buffer<Char> buffer);

	static vsm::result<string_view<Char>> copy_lexically_proximate(
		string_view<Char> string,
		string_buffer<Char> buffer);
#endif

	static path_combine_result_base<Char> combine(string_view<Char> lhs, string_view<Char> rhs);
};

extern template struct posix_path_iterator_base<char>;
extern template struct posix_path_iterator_base<wchar_t>;
extern template struct posix_path_iterator_base<char8_t>;
extern template struct posix_path_iterator_base<char16_t>;
extern template struct posix_path_iterator_base<char32_t>;

extern template struct posix_path_traits_base<char>;
extern template struct posix_path_traits_base<wchar_t>;
extern template struct posix_path_traits_base<char8_t>;
extern template struct posix_path_traits_base<char16_t>;
extern template struct posix_path_traits_base<char32_t>;

} // namespace detail

namespace posix {

template<typename Char>
struct path_traits : detail::posix_path_traits_base<Char>
{
	static constexpr Char preferred_separator = '/';

	static constexpr bool is_separator(Char const character) noexcept
	{
		return character == static_cast<Char>('/');
	}

	using iterator_base = detail::posix_path_iterator<Char>;
};

} // namespace posix
} // namespace allio
