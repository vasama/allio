#pragma once

#include <allio/detail/path.hpp>
#include <allio/detail/path_fwd.hpp>
#include <allio/detail/platform.h>
#include <allio/path_char.hpp>
#include <allio/path_literals.hpp>

#include <vsm/arrow.hpp>
#include <vsm/assert.h>
#include <vsm/preprocessor.h>

#include <compare>
#include <span>
#include <string_view>

namespace allio {

template<typename Char, typename Encoding = void>
class basic_path_view : public detail::path_encoding_base<Encoding>
{
public:
	using string_view_type = std::basic_string_view<Char>;

private:
	template<typename String>
	using path_template = basic_path_adaptor<Char, String, Encoding>;

	string_view_type m_string_view;

public:
	static constexpr Char preferred_separator = static_cast<Char>(
#if vsm_os_win32
		'\\'
#else
		'/'
#endif
	);

	[[nodiscard]] static constexpr bool is_separator(Char const character)
	{
		return character == static_cast<Char>('/')
#if vsm_os_win32
			|| character == static_cast<Char>('\\')
#endif
		;
	}


	using value_type = Char;

	class iterator
	{
		static constexpr bool Reverse = false;

		Char const* m_sbeg;
		Char const* m_send;
		Char const* m_beg;
		Char const* m_end;

	public:
		using iterator_category = std::bidirectional_iterator_tag;
		using value_type = basic_path_view;
		using difference_type = ptrdiff_t;
		using pointer = vsm::arrow<basic_path_view>;
		using reference = basic_path_view;

		[[nodiscard]] constexpr basic_path_view operator*() const
		{
			return basic_path_view(string_view_type(
				m_sbeg,
				static_cast<size_t>(m_send - m_sbeg)));
		}

		[[nodiscard]] constexpr pointer operator->() const
		{
			return basic_path_view(string_view_type(
				m_sbeg,
				static_cast<size_t>(m_send - m_sbeg)));
		}

		constexpr iterator& operator++() &
		{
			increment();
			return *this;
		}

		[[nodiscard]] constexpr iterator operator++(int) &
		{
			iterator it = *this;
			increment();
			return it;
		}

		constexpr iterator& operator--() &
		{
			decrement();
			return *this;
		}

		[[nodiscard]] constexpr iterator operator--(int) &
		{
			iterator it = *this;
			decrement();
			return it;
		}

		[[nodiscard]] friend constexpr bool operator==(iterator const& lhs, iterator const& rhs)
		{
			return lhs.m_sbeg == rhs.m_sbeg;
		}

		[[nodiscard]] friend constexpr bool operator!=(iterator const& lhs, iterator const& rhs)
		{
			return lhs.m_sbeg != rhs.m_sbeg;
		}

	private:
		constexpr void init_begin(Char const* const beg, Char const* const end);

		constexpr void increment();
		constexpr void decrement();

		static constexpr iterator make_begin(Char const* const beg, Char const* const end)
		{
			iterator it;
			it.m_sbeg = beg;
			if (beg != end)
			{
				it.init_begin(beg, end);
			}
			return it;
		}

		static constexpr iterator make_end(Char const* const beg, Char const* const end)
		{
			iterator it;
			it.m_beg = beg;
			it.m_end = end;
			it.m_sbeg = end;
			it.m_send = end;
			return it;
		}

		friend class basic_path_view;
	};
	using const_iterator = iterator;

	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;


	basic_path_view() = default;

	constexpr basic_path_view(basic_path_literal<Char> const literal)
		: m_string_view(literal.string())
	{
	}

	template<typename String>
	constexpr basic_path_view(path_template<String> const& path)
		: m_string_view(path.string())
	{
	}

	explicit constexpr basic_path_view(string_view_type const string)
		: m_string_view(string)
	{
	}

	explicit constexpr basic_path_view(Char const* const c_str)
		: m_string_view(c_str)
	{
	}

	explicit constexpr basic_path_view(Char const* const data, size_t const size)
		: m_string_view(data, size)
	{
	}

	template<std::contiguous_iterator Iterator, std::sized_sentinel_for<Iterator> Sentinel>
	explicit constexpr basic_path_view(Iterator const iterator, Sentinel const sentinel)
		//requires std::is_same_v<std::iterator_value_t<Iterator>, Char>
		: m_string_view(iterator, sentinel)
	{
	}

	template<std::ranges::contiguous_range Range>
	explicit constexpr basic_path_view(Range&& range)
		//requires std::is_same_v<std::ranges::range_value_t<Range>, Char>
		: m_string_view(static_cast<Range&&>(range))
	{
	}

	basic_path_view(decltype(nullptr)) = delete;


	[[nodiscard]] constexpr bool empty() const
	{
		return m_string_view.empty();
	}

	[[nodiscard]] constexpr string_view_type string() const
	{
		return m_string_view;
	}


	[[nodiscard]] constexpr bool is_absolute() const;

	[[nodiscard]] constexpr bool is_relative() const
	{
		return !is_absolute();
	}


	[[nodiscard]] constexpr basic_path_view root_name() const;
	[[nodiscard]] constexpr basic_path_view root_directory() const;
	[[nodiscard]] constexpr basic_path_view root_path() const;
	[[nodiscard]] constexpr basic_path_view relative_path() const;
	[[nodiscard]] constexpr basic_path_view parent_path() const;
	[[nodiscard]] constexpr basic_path_view filename() const;
	[[nodiscard]] constexpr basic_path_view stem() const;
	[[nodiscard]] constexpr basic_path_view extension() const;


	[[nodiscard]] constexpr bool has_root_name() const
	{
		return !root_name().empty();
	}

	[[nodiscard]] constexpr bool has_root_directory() const
	{
		return !root_directory().empty();
	}

	[[nodiscard]] constexpr bool has_root_path() const
	{
		return !root_path().empty();
	}

	[[nodiscard]] constexpr bool has_relative_path() const
	{
		return !relative_path().empty();
	}

	[[nodiscard]] constexpr bool has_parent_path() const
	{
		return !parent_path().empty();
	}

	[[nodiscard]] constexpr bool has_filename() const
	{
		return !filename().empty();
	}

	[[nodiscard]] constexpr bool has_stem() const
	{
		return !stem().empty();
	}

	[[nodiscard]] constexpr bool has_extension() const
	{
		return !extension().empty();
	}


	[[nodiscard]] constexpr bool has_trailing_separators() const;
	[[nodiscard]] constexpr basic_path_view without_trailing_separators() const;


	[[nodiscard]] constexpr basic_path_view copy_lexically_normal(Char* buffer) const;

	[[nodiscard]] constexpr basic_path_view copy_lexically_relative(
		basic_path_view base,
		Char* buffer) const;

	[[nodiscard]] constexpr basic_path_view copy_lexically_proximate(
		basic_path_view base,
		Char* buffer) const;


	[[nodiscard]] constexpr iterator begin() const
	{
		Char const* const data = m_string_view.data();
		return iterator::make_begin(data, data + m_string_view.size());
	}

	[[nodiscard]] constexpr iterator end() const
	{
		Char const* const data = m_string_view.data();
		return iterator::make_end(data, data + m_string_view.size());
	}

	[[nodiscard]] constexpr reverse_iterator rbegin() const
	{
		return reverse_iterator(end());
	}

	[[nodiscard]] constexpr reverse_iterator rend() const
	{
		return reverse_iterator(begin());
	}


	[[nodiscard]] constexpr std::strong_ordering compare(basic_path_view const other) const
	{
		return compare(*this, other);
	}

	[[nodiscard]] friend constexpr bool operator==(
		basic_path_view const lhs,
		basic_path_view const rhs)
	{
		return equal(lhs, rhs);
	}

	[[nodiscard]] friend constexpr bool operator!=(
		basic_path_view const lhs,
		basic_path_view const rhs)
	{
		return !equal(lhs, rhs);
	}

	[[nodiscard]] friend constexpr auto operator<=>(
		basic_path_view const lhs,
		basic_path_view const rhs)
	{
		return compare(lhs, rhs);
	}


#if 0
	//TODO: Implement lexically_equivalent
	/*[[nodiscard]]*/ friend constexpr bool lexically_equivalent(
		basic_path_view const lhs,
		basic_path_view const rhs);
#endif

private:
	static constexpr bool equal(basic_path_view lhs, basic_path_view rhs);
	static constexpr std::strong_ordering compare(basic_path_view lhs, basic_path_view rhs);

	friend string_view_type tag_invoke(get_path_string_t, basic_path_view const& self)
	{
		return self.m_string_view;
	}
};


//TODO: Make this variadic to allow combining N paths at once.
template<typename Char, typename Encoding = void>
class basic_path_combine_result
{
	std::basic_string_view<Char> m_lhs;
	std::basic_string_view<Char> m_rhs;
	bool m_requires_separator;

public:
	explicit constexpr basic_path_combine_result(
		basic_path_view<Char> const path,
		bool const requires_separator = false)
		: m_lhs(path.string())
		, m_requires_separator(requires_separator)
	{
	}

	explicit constexpr basic_path_combine_result(
		basic_path_view<Char> const lhs,
		basic_path_view<Char> const rhs,
		bool const requires_separator = false)
		: m_lhs(lhs.string())
		, m_rhs(rhs.string())
		, m_requires_separator(requires_separator)
	{
	}


	[[nodiscard]] constexpr size_t size() const
	{
		return m_lhs.size() + m_rhs.size() + m_requires_separator;
	}

	constexpr basic_path_view<Char> copy(std::span<Char> const buffer) const
	{
		vsm_assert(buffer.size() >= size()); //PRECONDITION

		Char* const out_beg = buffer.data();
		Char* out_end = out_beg;

		out_end = std::copy(m_lhs.data(), m_lhs.data() + m_lhs.size(), out_end);
		if (m_requires_separator)
		{
			*out_end++ = basic_path_view<Char>::preferred_separator;
		}
		out_end = std::copy(m_rhs.data(), m_rhs.data() + m_rhs.size(), out_end);

		return basic_path_view<Char>(out_beg, out_end);
	}
};

template<typename Char, typename Encoding>
[[nodiscard]] constexpr basic_path_combine_result<Char> combine_path(
	basic_path_view<Char, Encoding> lhs,
	basic_path_view<Char, Encoding> rhs);


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

#include <allio/linux/detail/undef.i>
#include vsm_pp_include(allio/vsm_os/detail/path_view.hpp)
#include <allio/linux/detail/undef.i>
