#pragma once

#include <allio/any_path_buffer.hpp>
#include <allio/detail/path.hpp>
#include <allio/detail/platform.h>
#include <allio/path_char.hpp>
#include <allio/path_literals.hpp>
#include <allio/path_traits.hpp>

#include <vsm/arrow.hpp>
#include <vsm/assert.h>
#include <vsm/preprocessor.h>

#include <compare>
#include <span>
#include <string_view>

#include <allio/linux/detail/undef.i>
#include vsm_pp_include(allio/vsm_os/path_traits.hpp)
#include <allio/linux/detail/undef.i>

namespace allio {

template<typename Char, typename Traits, typename Encoding = void>
class foreign_path_view : public detail::path_encoding_base<Encoding>
{
public:
	using string_view_type = std::basic_string_view<Char>;

private:
	string_view_type m_string_view;

	template<bool Forward>
	class basic_iterator : public Traits::iterator_base
	{
	public:
		using iterator_category = std::bidirectional_iterator_tag;
		using value_type = foreign_path_view;
		using difference_type = ptrdiff_t;
		using pointer = vsm::arrow<foreign_path_view>;
		using reference = foreign_path_view;

		[[nodiscard]] constexpr foreign_path_view operator*() const
		{
			return foreign_path_view(this->current());
		}

		[[nodiscard]] constexpr vsm::arrow<foreign_path_view> operator->() const
		{
			return foreign_path_view(this->current());
		}

		constexpr basic_iterator& operator++() &
		{
			if constexpr (Forward)
			{
				this->increment();
			}
			else
			{
				this->decrement();
			}
			return *this;
		}

		[[nodiscard]] constexpr basic_iterator operator++(int) &
		{
			basic_iterator it = *this;
			if constexpr (Forward)
			{
				this->increment();
			}
			else
			{
				this->decrement();
			}
			return it;
		}

		constexpr basic_iterator& operator--() &
		{
			if constexpr (Forward)
			{
				this->decrement();
			}
			else
			{
				this->increment();
			}
			return *this;
		}

		[[nodiscard]] constexpr basic_iterator operator--(int) &
		{
			basic_iterator it = *this;
			if constexpr (Forward)
			{
				this->decrement();
			}
			else
			{
				this->increment();
			}
			return it;
		}

		friend class foreign_path_view;
	};

public:
	static constexpr Char preferred_separator = Traits::preferred_separator;

	[[nodiscard]] static constexpr bool is_separator(Char const character)
	{
		return Traits::is_separator(character);
	}


	using value_type = Char;

	using iterator       = basic_iterator</* Forward: */ true>;
	using const_iterator = basic_iterator</* Forward: */ true>;

	using reverse_iterator       = basic_iterator</* Forward: */ false>;
	using const_reverse_iterator = basic_iterator</* Forward: */ false>;


	foreign_path_view() = default;

	constexpr foreign_path_view(basic_path_literal<Char> const literal)
		: m_string_view(literal.string())
	{
	}

	explicit constexpr foreign_path_view(string_view_type const string)
		: m_string_view(string)
	{
	}

	explicit foreign_path_view(decltype(nullptr) c_str) = delete;

	explicit constexpr foreign_path_view(Char const* const c_str)
		: m_string_view(c_str)
	{
	}

	explicit foreign_path_view(decltype(nullptr) c_str, size_t size) = delete;

	explicit constexpr foreign_path_view(Char const* const data, size_t const size)
		: m_string_view(data, size)
	{
	}

	template<std::contiguous_iterator Iterator, std::sized_sentinel_for<Iterator> Sentinel>
		requires std::is_same_v<std::iter_value_t<Iterator>, Char>
	explicit constexpr foreign_path_view(Iterator const iterator, Sentinel const sentinel)
		: m_string_view(iterator, sentinel)
	{
	}

	template<std::ranges::contiguous_range Range>
		requires std::is_same_v<std::ranges::range_value_t<Range>, Char>
	explicit constexpr foreign_path_view(Range&& range)
		: m_string_view(static_cast<Range&&>(range))
	{
	}

	template<path_like Path>
	explicit(!detail::compatible_container_encoding<Path, Encoding>)
	constexpr foreign_path_view(Path const& path)
		: m_string_view(get_path_string(path))
	{
	}


	[[nodiscard]] constexpr bool empty() const
	{
		return m_string_view.empty();
	}

	[[nodiscard]] constexpr string_view_type string() const
	{
		return m_string_view;
	}


	[[nodiscard]] bool is_absolute() const
	{
		return Traits::is_absolute(m_string_view);
	}

	[[nodiscard]] bool is_relative() const
	{
		return !Traits::is_absolute(m_string_view);
	}


	[[nodiscard]] foreign_path_view root_name() const
	{
		return foreign_path_view(Traits::root_name(m_string_view));
	}

	[[nodiscard]] foreign_path_view root_directory() const
	{
		return foreign_path_view(Traits::root_directory(m_string_view));
	}

	[[nodiscard]] foreign_path_view root_path() const
	{
		return foreign_path_view(Traits::root_path(m_string_view));
	}

	[[nodiscard]] foreign_path_view relative_path() const
	{
		return foreign_path_view(Traits::relative_path(m_string_view));
	}

	[[nodiscard]] foreign_path_view parent_path() const
	{
		return foreign_path_view(Traits::parent_path(m_string_view));
	}

	[[nodiscard]] foreign_path_view filename() const
	{
		return foreign_path_view(Traits::filename(m_string_view));
	}

	[[nodiscard]] foreign_path_view stem() const
	{
		return foreign_path_view(Traits::stem(m_string_view));
	}

	[[nodiscard]] foreign_path_view extension() const
	{
		return foreign_path_view(Traits::extension(m_string_view));
	}


	[[nodiscard]] bool has_root_name() const
	{
		return !Traits::root_name(m_string_view).empty();
	}

	[[nodiscard]] bool has_root_directory() const
	{
		return !Traits::root_directory(m_string_view).empty();
	}

	[[nodiscard]] bool has_root_path() const
	{
		return !Traits::root_path(m_string_view).empty();
	}

	[[nodiscard]] bool has_relative_path() const
	{
		return !Traits::relative_path(m_string_view).empty();
	}

	[[nodiscard]] bool has_parent_path() const
	{
		return !Traits::parent_path(m_string_view).empty();
	}

	[[nodiscard]] bool has_filename() const
	{
		return !Traits::filename(m_string_view).empty();
	}

	[[nodiscard]] bool has_stem() const
	{
		return !Traits::stem(m_string_view).empty();
	}

	[[nodiscard]] bool has_extension() const
	{
		return !Traits::extension(m_string_view).empty();
	}


	[[nodiscard]] bool has_trailing_separators() const
	{
		return Traits::has_trailing_separators(m_string_view);
	}

	[[nodiscard]] foreign_path_view without_trailing_separators() const
	{
		return foreign_path_view(Traits::without_trailing_separators(m_string_view));
	}


	[[nodiscard]] bool is_lexically_normal() const
	{
		return Traits::is_lexically_normal(m_string_view);
	}

	foreign_path_view copy_lexically_normal(path_buffer<Char, Encoding> const buffer) const
	{
		return foreign_path_view(detail::throw_on_error(Traits::copy_lexically_normal(
			m_string_view,
			buffer.string())));
	}

	foreign_path_view copy_lexically_relative(
		foreign_path_view const base,
		path_buffer<Char, Encoding> const buffer) const
	{
		return foreign_path_view(detail::throw_on_error(Traits::copy_lexically_relative(
			m_string_view,
			base.m_string_view,
			buffer.string())));
	}

	foreign_path_view copy_lexically_proximate(
		foreign_path_view const base,
		path_buffer<Char, Encoding> const buffer) const
	{
		return foreign_path_view(detail::throw_on_error(Traits::copy_lexically_proximate(
			m_string_view,
			base.m_string_view,
			buffer.string())));
	}


	[[nodiscard]] iterator begin() const
	{
		Char const* const beg = m_string_view.data();
		Char const* const end = beg + m_string_view.size();

		iterator it;
		it.set_reverse_end(beg, end);
		if (beg != end)
		{
			it.increment();
		}
		return it;
	}

	[[nodiscard]] iterator cbegin() const
	{
		Char const* const beg = m_string_view.data();
		Char const* const end = beg + m_string_view.size();

		iterator it;
		it.set_reverse_end(beg, end);
		if (beg != end)
		{
			it.increment();
		}
		return it;
	}

	[[nodiscard]] iterator end() const
	{
		Char const* const beg = m_string_view.data();
		Char const* const end = beg + m_string_view.size();

		iterator it;
		it.set_forward_end(beg, end);
		return it;
	}

	[[nodiscard]] iterator cend() const
	{
		Char const* const beg = m_string_view.data();
		Char const* const end = beg + m_string_view.size();

		iterator it;
		it.set_forward_end(beg, end);
		return it;
	}

	[[nodiscard]] reverse_iterator rbegin() const
	{
		Char const* const beg = m_string_view.data();
		Char const* const end = beg + m_string_view.size();

		reverse_iterator it;
		it.set_forward_end(beg, end);
		if (beg != end)
		{
			it.decrement();
		}
		return it;
	}

	[[nodiscard]] reverse_iterator crbegin() const
	{
		Char const* const beg = m_string_view.data();
		Char const* const end = beg + m_string_view.size();

		reverse_iterator it;
		it.set_forward_end(beg, end);
		if (beg != end)
		{
			it.decrement();
		}
		return it;
	}

	[[nodiscard]] reverse_iterator rend() const
	{
		Char const* const beg = m_string_view.data();
		Char const* const end = beg + m_string_view.size();

		reverse_iterator it;
		it.set_reverse_end(beg, end);
		return it;
	}

	[[nodiscard]] reverse_iterator crend() const
	{
		Char const* const beg = m_string_view.data();
		Char const* const end = beg + m_string_view.size();

		reverse_iterator it;
		it.set_reverse_end(beg, end);
		return it;
	}


	[[nodiscard]] std::strong_ordering compare(foreign_path_view const other) const
	{
		return Traits::compare(m_string_view, other.m_string_view);
	}

	[[nodiscard]] friend bool operator==(
		foreign_path_view const lhs,
		foreign_path_view const rhs)
	{
		return Traits::equal(lhs.m_string_view, rhs.m_string_view);
	}

	[[nodiscard]] friend bool operator!=(
		foreign_path_view const lhs,
		foreign_path_view const rhs)
	{
		return !Traits::equal(lhs.m_string_view, rhs.m_string_view);
	}

	[[nodiscard]] friend auto operator<=>(
		foreign_path_view const lhs,
		foreign_path_view const rhs)
	{
		return Traits::compare(lhs.m_string_view, rhs.m_string_view);
	}

private:
	friend string_view_type tag_invoke(get_path_string_t, foreign_path_view const& self)
	{
		return self.m_string_view;
	}
};

template<typename Char, typename Traits, typename Encoding>
[[nodiscard]] bool lexically_equivalent(
	foreign_path_view<Char, Traits, Encoding> const lhs,
	foreign_path_view<Char, Traits, Encoding> const rhs)
{
	return Traits::lexically_equivalent(lhs.string(), rhs.string());
}

template<typename Char, typename Encoding = void>
using basic_path_view = foreign_path_view<Char, native_path_traits<Char>, Encoding>;


template<typename Char, typename Traits, typename Encoding = void>
class foreign_path_combine_result : path_combine_result_base<Char>
{
	using path_view_type = foreign_path_view<Char, Traits, Encoding>;
	using string_view_type = std::basic_string_view<Char>;

public:
	explicit constexpr foreign_path_combine_result(path_combine_result_base<Char> const base)
		: path_combine_result_base<Char>(base)
	{
	}

	explicit constexpr foreign_path_combine_result(
		path_view_type const path,
		bool const requires_separator = false)
		: path_combine_result_base<Char>(path.string(), string_view_type(), requires_separator)
	{
	}

	explicit constexpr foreign_path_combine_result(
		path_view_type const lhs,
		path_view_type const rhs,
		bool const requires_separator = false)
		: path_combine_result_base<Char>(lhs.string(), rhs.string(), requires_separator)
	{
	}

	using path_combine_result_base<Char>::size;
	using path_combine_result_base<Char>::first;
	using path_combine_result_base<Char>::second;
	using path_combine_result_base<Char>::requires_separator;

	constexpr path_view_type copy(std::span<Char> const buffer) const
	{
		return path_view_type(path_combine_result_base<Char>::copy(
			buffer.data(),
			Traits::preferred_separator));
	}
};

template<typename Char, typename Traits, typename Encoding>
[[nodiscard]] foreign_path_combine_result<Char, Traits, Encoding> combine_path(
	foreign_path_view<Char, Traits, Encoding> const lhs,
	foreign_path_view<Char, Traits, Encoding> const rhs)
{
	return foreign_path_combine_result<Char, Traits, Encoding>(Traits::combine(
		lhs.string(),
		rhs.string()));
}

template<typename Char, typename Encoding = void>
using basic_path_combine_result = foreign_path_combine_result<
	Char,
	native_path_traits<Char>,
	Encoding>;


using path_view = basic_path_view<char>;
using wpath_view = basic_path_view<wchar_t>;
using u8path_view = basic_path_view<char8_t>;
using u16path_view = basic_path_view<char16_t>;
using u32path_view = basic_path_view<char32_t>;

using path_combine_result = basic_path_combine_result<char>;
using wpath_combine_result = basic_path_combine_result<wchar_t>;
using u8path_combine_result = basic_path_combine_result<char8_t>;
using u16path_combine_result = basic_path_combine_result<char16_t>;
using u32path_combine_result = basic_path_combine_result<char32_t>;

using native_path_view = basic_path_view<native_path_char_t, no_encoding_t>;
using native_path_combine_result = basic_path_combine_result<native_path_char_t, no_encoding_t>;

// TODO: Get rid of these and use native_path_view instead.
using platform_path_view = native_path_view;
using platform_path_combine_result = native_path_combine_result;

} // namespace allio
