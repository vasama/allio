#pragma once

#include <allio/detail/platform.h>
#include <allio/path_literal.hpp>

#include <vsm/box.hpp>
#include <vsm/preprocessor.h>

#include <string_view>

namespace allio {

template<typename Char, typename String>
class basic_path_adaptor;

template<typename Char>
class basic_path_view : std::basic_string_view<Char>
{
	template<typename String>
	using path_template = basic_path_adaptor<Char, String>;

	using string_view_type = std::basic_string_view<Char>;

public:
	static constexpr Char preferred_separator = static_cast<Char>(
#if vsm_os_win32
		'\\'
#else
		'/'
#endif
	);

	static constexpr bool is_separator(Char const character)
	{
		return character == static_cast<Char>('/')
#if vsm_os_win32
			|| character == static_cast<Char>('\\')
#endif
		;
	}


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
		using pointer = vsm::box<basic_path_view>;
		using reference = basic_path_view;

		constexpr basic_path_view operator*() const
		{
			return basic_path_view(string_view_type(m_sbeg, m_send - m_sbeg));
		}

		constexpr pointer operator->() const
		{
			return basic_path_view(string_view_type(m_sbeg, m_send - m_sbeg));
		}

		constexpr iterator& operator++() &
		{
			increment();
			return *this;
		}

		constexpr iterator operator++(int) &
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

		constexpr iterator operator--(int) &
		{
			iterator it = *this;
			decrement();
			return it;
		}

		friend constexpr bool operator==(iterator const& lhs, iterator const& rhs)
		{
			return lhs.m_sbeg == rhs.m_sbeg;
		}

		friend constexpr bool operator!=(iterator const& lhs, iterator const& rhs)
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

	template<typename String>
	constexpr basic_path_view(path_template<String> const& path)
		: string_view_type(path.string())
	{
	}

	explicit constexpr basic_path_view(string_view_type const string)
		: string_view_type(string)
	{
	}

	constexpr basic_path_view(Char const* const c_str)
		: string_view_type(c_str)
	{
	}

	explicit constexpr basic_path_view(Char const* const data, size_t const size)
		: string_view_type(data, size)
	{
	}

	template<std::contiguous_iterator Iterator, std::sized_sentinel_for<Iterator> Sentinel>
	explicit constexpr basic_path_view(Iterator const iterator, Sentinel const sentinel)
		//requires std::is_same_v<std::iterator_value_t<Iterator>, Char>
		: string_view_type(iterator, sentinel)
	{
	}

	template<std::ranges::contiguous_range Range>
	explicit constexpr basic_path_view(Range&& range)
		//requires std::is_same_v<std::ranges::range_value_t<Range>, Char>
		: string_view_type(static_cast<Range&&>(range))
	{
	}

	basic_path_view(decltype(nullptr)) = delete;


	using string_view_type::empty;

	constexpr string_view_type string() const
	{
		return *this;
	}


	constexpr bool is_absolute() const;

	constexpr bool is_relative() const
	{
		return !is_absolute();
	}


	constexpr basic_path_view root_name() const;
	constexpr basic_path_view root_directory() const;
	constexpr basic_path_view root_path() const;
	constexpr basic_path_view relative_path() const;
	constexpr basic_path_view parent_path() const;
	constexpr basic_path_view filename() const;
	constexpr basic_path_view stem() const;
	constexpr basic_path_view extension() const;


	constexpr bool has_root_name() const
	{
		return !root_name().empty();
	}

	constexpr bool has_root_directory() const
	{
		return !root_directory().empty();
	}

	constexpr bool has_root_path() const
	{
		return !root_path().empty();
	}

	constexpr bool has_relative_path() const
	{
		return !relative_path().empty();
	}

	constexpr bool has_parent_path() const
	{
		return !parent_path().empty();
	}

	constexpr bool has_filename() const
	{
		return !filename().empty();
	}

	constexpr bool has_stem() const
	{
		return !stem().empty();
	}

	constexpr bool has_extension() const
	{
		return !extension().empty();
	}


	constexpr bool has_trailing_separators() const;
	constexpr basic_path_view without_trailing_separators() const;


	constexpr basic_path_view copy_lexically_normal(Char* buffer) const;
	constexpr basic_path_view copy_lexically_relative(basic_path_view base, Char* buffer) const;
	constexpr basic_path_view copy_lexically_proximate(basic_path_view base, Char* buffer) const;


	constexpr iterator begin() const
	{
		Char const* const data = this->data();
		return iterator::make_begin(data, data + this->size());
	}

	constexpr iterator end() const
	{
		Char const* const data = this->data();
		return iterator::make_end(data, data + this->size());
	}

	constexpr reverse_iterator rbegin() const
	{
		return reverse_iterator(end());
	}

	constexpr reverse_iterator rend() const
	{
		return reverse_iterator(begin());
	}


	constexpr int compare(basic_path_view const other) const
	{
		return compare(*this, other);
	}

	friend constexpr bool operator==(basic_path_view const lhs, basic_path_view const rhs)
	{
		return equal(lhs, rhs);
	}

	friend constexpr bool operator!=(basic_path_view const lhs, basic_path_view const rhs)
	{
		return !equal(lhs, rhs);
	}

	friend constexpr auto operator<=>(basic_path_view const lhs, basic_path_view const rhs)
	{
		return compare(lhs, rhs) <=> 0;
	}


	friend constexpr bool lexically_equivalent(basic_path_view lhs, basic_path_view rhs);

private:
	static constexpr bool equal(basic_path_view lhs, basic_path_view rhs);
	static constexpr int compare(basic_path_view lhs, basic_path_view rhs);
};


template<typename Char>
class basic_path_combine_result
{
	basic_path_view<Char> m_lhs;
	basic_path_view<Char> m_rhs;
	bool m_requires_separator;

public:
	explicit constexpr basic_path_combine_result(basic_path_view<Char> const path, bool const requires_separator = false)
		: m_lhs(path), m_requires_separator(requires_separator)
	{
	}

	explicit constexpr basic_path_combine_result(basic_path_view<Char> const lhs, basic_path_view<Char> const rhs, bool const requires_separator = false)
		: m_lhs(lhs), m_rhs(rhs), m_requires_separator(requires_separator)
	{
	}


	constexpr size_t size() const
	{
		return m_lhs.string().size() + m_rhs.string().size() + m_requires_separator;
	}

	constexpr basic_path_view<Char> copy(Char* const beg) const
	{
		size_t size = 0;
		size += m_lhs.string().copy(beg + size, m_lhs.string().size());
		if (m_requires_separator)
		{
			beg[size++] = basic_path_view<Char>::preferred_separator;
		}
		size += m_rhs.string().copy(beg + size, m_rhs.string().size());
		return basic_path_view<Char>(std::basic_string_view<Char>(beg, size));
	}
};

template<typename Char>
constexpr basic_path_combine_result<Char> combine_path(basic_path_view<Char> lhs, basic_path_view<Char> rhs);


using path_view = basic_path_view<char>;
using path_combine_result = basic_path_combine_result<char>;

} // namespace allio

#include <allio/linux/detail/undef.i>
#include vsm_pp_include(allio/vsm_os/detail/path_view.hpp)
#include <allio/linux/detail/undef.i>
