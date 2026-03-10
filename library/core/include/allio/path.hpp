#pragma once

#include <allio/detail/mutable_buffer.hpp>
#include <allio/path_view.hpp>

#include <string>

namespace allio {
namespace detail {

template<typename T>
struct has_resize_and_overwrite_helper
{
	size_t operator()(T*, size_t) const;
};

template<typename Container>
concept has_resize_and_overwrite = requires (Container& container)
{
	container.resize_and_overwrite(
		static_cast<size_t>(0),
		has_resize_and_overwrite_helper<typename Container::value_type>());
};

template<typename Traits, typename String, typename Char>
void combine_path_in_place(String& lhs, std::basic_string_view<Char> const rhs)
{
	auto const combined = Traits::combine(lhs, rhs);

	auto const l = combined.first();
	auto const l_size = l.size();

	auto const r = combined.second();
	auto const r_size = r.size();

	auto l_beg = l.data();
	auto l_end = l_beg + l_size;

	auto const r_beg = r.data();
	auto const r_end = r_beg + r_size;

	size_t l_off = 0;
	{
		auto const s_beg = lhs.data();
		auto const s_end = s_beg + lhs.size();

		if (std::less_equal()(s_beg, l_beg) && std::less_equal()(l_end, s_end))
		{
			l_off = static_cast<size_t>(l_beg - s_beg);
			l_beg = nullptr;
			l_end = nullptr;
		}
	}

	lhs.resize(combined.size());
	char* out = lhs.data();

	if (l_beg == nullptr)
	{
		l_beg = out + l_off;
		l_end = out + l_size;
	}

	if (l_beg == out)
	{
		out += l_size;
	}
	else
	{
		out = std::copy(l_beg, l_end, out);
	}

	if (combined.requires_separator())
	{
		*out++ = Traits::preferred_separator;
	}

	out = std::copy(r_beg, r_end, out);

	vsm_assert(out == lhs.data() + lhs.size());
}

} // namespace detail

template<typename Char, typename String, typename Traits, typename Encoding = void>
class foreign_path_adaptor : public detail::path_encoding_base<Encoding>
{
	static_assert(std::is_same_v<Char, typename String::value_type>);

	using path_view_type = foreign_path_view<Char, Traits, Encoding>;
	using string_view_type = std::basic_string_view<Char>;

	String m_string;

public:
	using string_type = String;


	static constexpr Char preferred_separator = Traits::preferred_separator;

	[[nodiscard]] static constexpr bool is_separator(Char const character)
	{
		return Traits::is_separator(character);
	}


	using iterator = typename path_view_type::iterator;
	using const_iterator = typename path_view_type::const_iterator;

	using reverse_iterator = typename path_view_type::reverse_iterator;
	using const_reverse_iterator = typename path_view_type::const_reverse_iterator;


	foreign_path_adaptor() = default;

	constexpr foreign_path_adaptor(path_view_type const path)
		: m_string(path.string())
	{
	}

	explicit constexpr foreign_path_adaptor(string_view_type const string)
		: m_string(string)
	{
	}

	explicit constexpr foreign_path_adaptor(string_type&& string)
		: m_string(static_cast<string_type&&>(string))
	{
	}

	explicit constexpr foreign_path_adaptor(string_type const& string)
		: m_string(string)
	{
	}

	explicit foreign_path_adaptor(decltype(nullptr)) = delete;

	explicit constexpr foreign_path_adaptor(Char const* const c_str)
		: m_string(c_str)
	{
	}

	explicit constexpr foreign_path_adaptor(Char const* const data, size_t const size)
		: m_string(data, size)
	{
	}

	template<std::input_iterator Iterator, std::sentinel_for<Iterator> Sentinel>
	explicit constexpr foreign_path_adaptor(Iterator const iterator, Sentinel const sentinel)
		//requires std::is_same_v<std::iterator_value_t<Iterator>, Char>
		: m_string(iterator, sentinel)
	{
	}

	template<std::ranges::input_range Range>
	explicit constexpr foreign_path_adaptor(Range&& range)
		//requires std::is_same_v<std::ranges::range_value_t<Range>, Char>
		: m_string(static_cast<Range&&>(range))
	{
	}

	foreign_path_adaptor(basic_path_combine_result<Char, Encoding> const& combine_result)
	{
		if constexpr (detail::has_resize_and_overwrite<String>)
		{
			m_string.resize_and_overwrite(
				combine_result.size(),
				[&](Char* const data, size_t const size)
				{
					return combine_result.copy(std::span<Char>(data, size)).string().size();
				});
		}
		else
		{
			m_string.resize(combine_result.size());
			combine_result.copy(m_string);
		}
	}


	[[nodiscard]] constexpr bool empty() const
	{
		return m_string.empty();
	}


	[[nodiscard]] constexpr path_view_type view() const
	{
		return path_view_type(m_string);
	}

	[[nodiscard]] constexpr string_type& string()
	{
		return m_string;
	}

	[[nodiscard]] constexpr string_type const& string() const
	{
		return m_string;
	}


	[[nodiscard]] bool is_absolute() const
	{
		return Traits::is_absolute(m_string);
	}

	[[nodiscard]] bool is_relative() const
	{
		return !Traits::is_absolute(m_string);
	}


	[[nodiscard]] foreign_path_adaptor root_name() const
	{
		return foreign_path_adaptor(Traits::root_name(m_string));
	}

	[[nodiscard]] foreign_path_adaptor root_directory() const
	{
		return foreign_path_adaptor(Traits::root_directory(m_string));
	}

	[[nodiscard]] foreign_path_adaptor root_path() const
	{
		return foreign_path_adaptor(Traits::root_path(m_string));
	}

	[[nodiscard]] foreign_path_adaptor relative_path() const
	{
		return foreign_path_adaptor(Traits::relative_path(m_string));
	}

	[[nodiscard]] foreign_path_adaptor parent_path() const
	{
		return foreign_path_adaptor(Traits::parent_path(m_string));
	}

	[[nodiscard]] foreign_path_adaptor filename() const
	{
		return foreign_path_adaptor(Traits::filename(m_string));
	}

	[[nodiscard]] foreign_path_adaptor stem() const
	{
		return foreign_path_adaptor(Traits::stem(m_string));
	}

	[[nodiscard]] foreign_path_adaptor extension() const
	{
		return foreign_path_adaptor(Traits::extension(m_string));
	}


	[[nodiscard]] bool has_root_name() const
	{
		return !Traits::root_name(m_string).empty();
	}

	[[nodiscard]] bool has_root_directory() const
	{
		return !Traits::root_directory(m_string).empty();
	}

	[[nodiscard]] bool has_root_path() const
	{
		return !Traits::root_path(m_string).empty();
	}

	[[nodiscard]] bool has_relative_path() const
	{
		return !Traits::relative_path(m_string).empty();
	}

	[[nodiscard]] bool has_parent_path() const
	{
		return !Traits::parent_path(m_string).empty();
	}

	[[nodiscard]] bool has_filename() const
	{
		return !Traits::filename(m_string).empty();
	}

	[[nodiscard]] bool has_stem() const
	{
		return !Traits::stem(m_string).empty();
	}

	[[nodiscard]] bool has_extension() const
	{
		return !Traits::extension(m_string).empty();
	}


	[[nodiscard]] bool has_trailing_separators() const
	{
		return Traits::has_trailing_separators(m_string);
	}

	[[nodiscard]] foreign_path_adaptor without_trailing_separators() const
	{
		return foreign_path_adaptor(Traits::without_trailing_separators(m_string));
	}

	void remove_trailing_separators()
	{
		m_string.resize(Traits::without_trailing_separators(m_string).size());
	}


	[[nodiscard]] bool is_lexically_normal() const
	{
		return Traits::is_lexically_normal(m_string);
	}

	path_view_type copy_lexically_normal(path_buffer<Char, Encoding> const buffer) const
	{
		return path_view_type(detail::throw_on_error(Traits::copy_lexically_normal(
			m_string,
			buffer.string())));
	}

	[[nodiscard]] foreign_path_adaptor lexically_normal() const
	{
		foreign_path_adaptor result;
		auto& string = result.m_string;

		if constexpr (detail::has_resize_and_overwrite<String>)
		{
			string.resize_and_overwrite(
				m_string.size(),
				[&](Char* const data, size_t const size)
				{
					return detail::throw_on_error(Traits::copy_lexically_normal(
						m_string,
						std::span(string))).size();
				});
		}
		else
		{
			string.resize(m_string.size());
			string.resize(detail::throw_on_error(Traits::copy_lexically_normal(
				m_string,
				std::span(string))).size());
		}
		return result;
	}

	path_view_type copy_lexically_relative(
		path_view_type const base,
		path_buffer<Char, Encoding> const buffer) const
	{
		return path_view_type(detail::throw_on_error(Traits::copy_lexically_relative(
			m_string,
			base.m_string_view,
			buffer.string())));
	}

	[[nodiscard]] foreign_path_adaptor lexically_relative(path_view_type const base) const
	{
		foreign_path_adaptor result;
		result.resize(Traits::copy_lexically_relative(base, result).string().size());
		return result;
	}

	path_view_type copy_lexically_proximate(
		path_view_type const base,
		path_buffer<Char, Encoding> const buffer) const
	{
		return path_view_type(detail::throw_on_error(Traits::copy_lexically_proximate(
			m_string,
			base.m_string_view,
			buffer.string())));
	}

	[[nodiscard]] foreign_path_adaptor lexically_proximate(path_view_type const base) const
	{
		foreign_path_adaptor result;
		result.resize(Traits::copy_lexically_proximate(base, result).string().size());
		return result;
	}


	[[nodiscard]] friend foreign_path_adaptor operator/(
		std::same_as<foreign_path_adaptor> auto const& lhs,
		std::same_as<foreign_path_adaptor> auto const& rhs)
	{
		return foreign_path_adaptor(combine_path(path_view_type(lhs), path_view_type(rhs)));
	}

	[[nodiscard]] friend foreign_path_adaptor operator/(
		foreign_path_adaptor const& lhs,
		path_view_type const rhs)
	{
		return foreign_path_adaptor(combine_path(path_view_type(lhs), rhs));
	}

	[[nodiscard]] friend foreign_path_adaptor operator/(
		path_view_type const lhs,
		foreign_path_adaptor const& rhs)
	{
		return foreign_path_adaptor(combine_path(lhs, path_view_type(rhs)));
	}

	foreign_path_adaptor& operator/=(foreign_path_adaptor const& rhs) &
	{
		detail::combine_path_in_place<Traits>(m_string, rhs.m_string);
		return *this;
	}

	foreign_path_adaptor& operator/=(path_view_type const rhs) &
	{
		detail::combine_path_in_place<Traits>(m_string, rhs.string());
		return *this;
	}


	[[nodiscard]] iterator begin() const
	{
		return path_view_type(m_string).begin();
	}

	[[nodiscard]] iterator cbegin() const
	{
		return path_view_type(m_string).begin();
	}

	[[nodiscard]] iterator end() const
	{
		return path_view_type(m_string).end();
	}

	[[nodiscard]] iterator cend() const
	{
		return path_view_type(m_string).end();
	}

	[[nodiscard]] reverse_iterator rbegin() const
	{
		return path_view_type(m_string).rbegin();
	}

	[[nodiscard]] reverse_iterator crbegin() const
	{
		return path_view_type(m_string).rbegin();
	}

	[[nodiscard]] reverse_iterator rend() const
	{
		return path_view_type(m_string).rend();
	}

	[[nodiscard]] reverse_iterator crend() const
	{
		return path_view_type(m_string).rend();
	}


	[[nodiscard]] std::strong_ordering compare(foreign_path_adaptor const& other) const
	{
		return Traits::compare(m_string, other.m_string);
	}

	[[nodiscard]] std::strong_ordering compare(path_view_type const other) const
	{
		return Traits::compare(m_string, other.string());
	}

	[[nodiscard]] friend bool operator==(
		foreign_path_adaptor const& lhs,
		foreign_path_adaptor const& rhs)
	{
		return Traits::equal(lhs.m_string, rhs.m_string);
	}

	[[nodiscard]] friend bool operator==(
		foreign_path_adaptor const& lhs,
		path_view_type const rhs)
	{
		return Traits::equal(lhs.m_string, rhs.string());
	}

	[[nodiscard]] friend bool operator==(
		path_view_type const lhs,
		foreign_path_adaptor const& rhs)
	{
		return Traits::equal(lhs.string(), rhs.m_string);
	}

	[[nodiscard]] friend bool operator!=(
		foreign_path_adaptor const& lhs,
		foreign_path_adaptor const& rhs)
	{
		return !Traits::equal(lhs.m_string, rhs.m_string);
	}

	[[nodiscard]] friend bool operator!=(
		foreign_path_adaptor const& lhs,
		path_view_type const rhs)
	{
		return !Traits::equal(lhs.m_string, rhs.string());
	}

	[[nodiscard]] friend bool operator!=(
		path_view_type const lhs,
		foreign_path_adaptor const& rhs)
	{
		return !Traits::equal(lhs.string(), rhs.m_string);
	}

	[[nodiscard]] friend auto operator<=>(
		foreign_path_adaptor const& lhs,
		foreign_path_adaptor const& rhs)
	{
		return Traits::compare(lhs.m_string, rhs.m_string);
	}

	[[nodiscard]] friend auto operator<=>(
		foreign_path_adaptor const& lhs,
		path_view_type const rhs)
	{
		return Traits::compare(lhs.m_string, rhs.string());
	}

	[[nodiscard]] friend auto operator<=>(
		path_view_type const lhs,
		foreign_path_adaptor const& rhs)
	{
		return Traits::compare(lhs.string(), rhs.m_string);
	}

private:
	template<vsm::any_cvref_of<foreign_path_adaptor> Self>
	friend vsm::copy_cvref_t<Self, string_type>&& tag_invoke(get_path_string_t, Self&& self)
	{
		return static_cast<vsm::copy_cvref_t<Self&&, string_type>>(self.m_string);
	}
};

template<typename Char, typename String, typename Traits, typename Encoding>
[[nodiscard]] bool lexically_equivalent(
	foreign_path_adaptor<Char, String, Traits, Encoding> const& lhs,
	foreign_path_adaptor<Char, String, Traits, Encoding> const& rhs)
{
	return Traits::lexically_equivalent(lhs.string(), rhs.string());
}

template<typename Char, typename String, typename Traits, typename Encoding>
[[nodiscard]] bool lexically_equivalent(
	foreign_path_adaptor<Char, String, Traits, Encoding> const& lhs,
	foreign_path_view<Char, Traits, Encoding> const& rhs)
{
	return Traits::lexically_equivalent(lhs.string(), rhs.string());
}

template<typename Char, typename String, typename Traits, typename Encoding>
[[nodiscard]] bool lexically_equivalent(
	foreign_path_view<Char, Traits, Encoding> const& lhs,
	foreign_path_adaptor<Char, String, Traits, Encoding> const& rhs)
{
	return Traits::lexically_equivalent(lhs.string(), rhs.string());
}

template<typename Path = void, typename Char, typename Traits, typename Encoding>
[[nodiscard]] auto lexically_normal(foreign_path_view<Char, Traits, Encoding> const path)
{
	using path_type = vsm::select_t<
		std::is_void_v<Path>,
		foreign_path_adaptor<Char, std::basic_string<Char>, Traits, Encoding>,
		Path>;

	path_type buffer;
	path.copy_lexically_normal(buffer);
	return buffer;
}

template<typename Path = void, typename Char, typename Traits, typename Encoding>
[[nodiscard]] auto lexically_relative(
	foreign_path_view<Char, Traits, Encoding> const path,
	foreign_path_view<Char, Traits, Encoding> const base)
{
	using path_type = vsm::select_t<
		std::is_void_v<Path>,
		foreign_path_adaptor<Char, std::basic_string<Char>, Traits, Encoding>,
		Path>;

	path_type buffer;
	path.copy_lexically_relative(base, buffer);
	return buffer;
}

template<typename Path = void, typename Char, typename Traits, typename Encoding>
[[nodiscard]] auto lexically_proximate(
	foreign_path_view<Char, Traits, Encoding> const path,
	foreign_path_view<Char, Traits, Encoding> const base)
{
	using path_type = vsm::select_t<
		std::is_void_v<Path>,
		foreign_path_adaptor<Char, std::basic_string<Char>, Traits, Encoding>,
		Path>;

	path_type buffer;
	path.copy_lexically_proximate(base, buffer);
	return buffer;
}

template<typename Char, typename String, typename Encoding = void>
using basic_path_adaptor = foreign_path_adaptor<Char, String, native_path_traits<Char>, Encoding>;

template<
	typename Char,
	typename Allocator = std::allocator<Char>,
	typename Encoding = void>
using basic_path = basic_path_adaptor<
	Char,
	std::basic_string<Char, std::char_traits<Char>, Allocator>,
	Encoding>;


template<typename Char, typename LhsString, typename RhsString, typename Traits, typename Encoding>
[[nodiscard]] foreign_path_combine_result<Char, Traits, Encoding> combine_path(
	foreign_path_adaptor<Char, LhsString, Traits, Encoding> const& lhs,
	foreign_path_adaptor<Char, RhsString, Traits, Encoding> const& rhs)
{
	return foreign_path_combine_result<Char, Traits, Encoding>(Traits::combine(
		lhs.string(),
		rhs.string()));
}

template<typename Char, typename String, typename Traits, typename Encoding>
[[nodiscard]] foreign_path_combine_result<Char, Traits, Encoding> combine_path(
	foreign_path_adaptor<Char, String, Traits, Encoding> const& lhs,
	foreign_path_view<Char, Traits, Encoding> const rhs)
{
	return foreign_path_combine_result<Char, Traits, Encoding>(Traits::combine(
		lhs.string(),
		rhs.string()));
}

template<typename Char, typename String, typename Traits, typename Encoding>
[[nodiscard]] foreign_path_combine_result<Char, Traits, Encoding> combine_path(
	foreign_path_view<Char, Traits, Encoding> const lhs,
	foreign_path_adaptor<Char, String, Traits, Encoding> const& rhs)
{
	return foreign_path_combine_result<Char, Traits, Encoding>(Traits::combine(
		lhs.string(),
		rhs.string()));
}


using path = basic_path<char>;
using wpath = basic_path<wchar_t>;
using u8path = basic_path<char8_t>;
using u16path = basic_path<char16_t>;
using u32path = basic_path<char32_t>;

template<typename String>
using native_path_adaptor = basic_path_adaptor<native_path_char_t, String, no_encoding_t>;

template<typename Allocator>
using basic_native_path = basic_path<native_path_char_t, Allocator, no_encoding_t>;

using native_path = basic_native_path<std::allocator<native_path_char_t>>;

//TODO: Get rid of this, use native_path instead.
using platform_path = native_path;

} // namespace allio
