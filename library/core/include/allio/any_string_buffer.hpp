#pragma once

#include <allio/detail/mutable_buffer.hpp>
#include <allio/encoding.hpp>
#include <allio/error.hpp>

#include <vsm/assert.h>
#include <vsm/result.hpp>
#include <vsm/standard.hpp>
#include <vsm/tag_invoke.hpp>

#include <span>

namespace allio {
namespace detail {

template<typename String>
concept _any_mutable_string =
	mutable_contiguous_range<String> &&
	character<typename std::remove_cvref_t<String>::value_type>;

template<typename String>
concept any_mutable_string = _any_mutable_string<mutable_range_from_t<String>>;

template<typename String, typename Char>
concept _mutable_string_of =
	_any_mutable_string<String> &&
	std::is_same_v<typename std::remove_cvref_t<String>::value_type, Char>;

template<typename String, typename Char>
concept mutable_string_of = _mutable_string_of<mutable_range_from_t<String>, Char>;


struct string_buffer_base
{
	static constexpr size_t ctrl_bits = sizeof(size_t) * CHAR_BIT;

	static constexpr size_t utfx_bits = 1;
	static constexpr size_t utfx_mask = (static_cast<size_t>(1) << utfx_bits) - 1;

	static constexpr size_t type_bits = 3;
	static constexpr size_t type_mask = (static_cast<size_t>(1) << type_bits) - 1;

	static constexpr size_t size_bits = ctrl_bits - type_bits - utfx_bits;
	static constexpr size_t size_mask = (static_cast<size_t>(1) << size_bits) - 1;

	static constexpr size_t size_shift = 0;
	static constexpr size_t type_shift = size_shift + size_bits;
	static constexpr size_t utfx_shift = type_shift + type_bits;


	template<detail::character Char>
	static constexpr size_t type_bits_for =
		static_cast<size_t>(detail::char_type_of<Char>) << size_bits;

	struct buffer
	{
		void* data;
		size_t size;
	};

	using resize_type = vsm::result<buffer>(
		string_buffer_base const& self,
		size_t min_size,
		size_t max_size);

	void* m_data;
	size_t m_ctrl;
	resize_type* m_resize;

	template<character Char>
	explicit string_buffer_base(Char* const data, size_t const size, encoding_family const encoding)
		: m_data(data)
		, m_ctrl(static_cast<size_t>(size < size_mask ? size : size_mask) | type_bits_for<Char>)
		, m_resize(_resize_span<Char>)
	{
	}

	template<mutable_contiguous_range Container>
	explicit string_buffer_base(Container&& container, encoding_family const encoding)
		: string_buffer_base(container.data(), container.size())
	{
	}

	template<resizable_container Container>
	explicit string_buffer_base(Container&& container, encoding_family const encoding)
		: m_data(&container)
		, m_ctrl(type_bits_for<typename std::remove_cvref_t<Container>::value_type>)
		, m_resize(_resize_container<Container>)
	{
	}

	[[nodiscard]] allio::char_type char_type() const
	{
		return static_cast<allio::char_type>((m_ctrl >> type_shift) & type_mask);
	}

	[[nodiscard]] allio::encoding_family encoding() const
	{
		return static_cast<allio::encoding_family>((m_ctrl >> utfx_shift) & utfx_mask);
	}

	template<typename Char>
	vsm::result<std::span<Char>> _resize(
		size_t const min_size,
		size_t const max_size) const
	{
		vsm_try(buffer, m_resize(*this, min_size, max_size));
		return std::span<Char>(static_cast<Char*>(buffer.data), buffer.size);
	}

	template<typename Char>
	static vsm::result<buffer> _resize_span(
		string_buffer_base const& self,
		size_t const min_size,
		size_t const max_size)
	{
		size_t const size = self.m_ctrl & size_mask;
		if (min_size > size)
		{
			return vsm::unexpected(error::no_buffer_space);
		}
		return buffer{ self.m_data, std::min(size, max_size) };
	}

	template<typename Container>
	static vsm::result<buffer> _resize_container(
		string_buffer_base const& self,
		size_t const min_size,
		size_t const max_size)
	{
		auto& container = *static_cast<vsm::remove_ref_t<Container>*>(self.m_data);
		vsm_try_discard(resize_container(container, min_size, max_size));
		return buffer{ std::ranges::data(container), std::ranges::size(container) };
	}


	static string_buffer_base const& get(auto const& string_buffer)
	{
		return string_buffer;
	}
};

} // namespace detail

template<detail::character Char>
class string_buffer : detail::string_buffer_base
{
public:
	using value_type = Char;

	string_buffer()
		: string_buffer_base(static_cast<Char*>(nullptr), 0)
	{
	}

	explicit string_buffer(detail::string_buffer_base const& buffer)
		: string_buffer_base(buffer)
	{
		vsm_assert(sizeof(Char) == detail::get_char_type_size(buffer.char_type())); //PRECONDITION
	}

	template<detail::explicit_encoding_for<Char> Encoding = detail::default_encoding_t>
	string_buffer(Char* const data, size_t const size, Encoding const encoding = Encoding())
		: string_buffer_base(data, size, detail::get_encoding<Char>(encoding))
	{
	}

	template<
		std::contiguous_iterator Iterator,
		std::sized_sentinel_for<Iterator> Sentinel,
		detail::explicit_encoding_for<Char> Encoding = detail::default_encoding_t>
	string_buffer(Iterator const begin, Sentinel const end, Encoding const encoding = Encoding())
		: string_buffer_base(
			std::to_address(begin),
			//TODO: This static_cast could theoretically truncate.
			static_cast<size_t>(end - begin),
			detail::get_encoding<Char>(encoding))
	{
	}

	template<size_t Size, detail::explicit_encoding_for<Char> Encoding = detail::default_encoding_t>
	string_buffer(Char(&array)[Size], Encoding const encoding = Encoding())
		: string_buffer_base(array, Size, detail::get_encoding<Char>(encoding))
	{
	}

	template<
		detail::mutable_string_of<Char> String,
		detail::explicit_encoding_for<Char, String> Encoding = detail::default_encoding_t>
	string_buffer(String& string, Encoding const encoding = Encoding())
		: string_buffer_base(
			detail::get_mutable_range(string),
			detail::get_encoding<Char, String>(encoding))
	{
	}

	template<
		detail::mutable_string_of<Char> String,
		detail::explicit_encoding_for<Char, String> Encoding = detail::default_encoding_t>
	string_buffer(String const& string, Encoding const encoding = Encoding())
		: string_buffer_base(
			detail::get_mutable_range(string),
			detail::get_encoding<Char, String>(encoding))
	{
	}


	using string_buffer_base::encoding;

	[[nodiscard]] vsm::result<std::span<Char>> resize(size_t const size) const
	{
		return string_buffer_base::_resize<Char>(size, size);
	}

	[[nodiscard]] vsm::result<std::span<Char>> resize(
		size_t const min_size,
		size_t const max_size) const
	{
		vsm_assert(min_size <= max_size); //PRECONDITION
		return string_buffer_base::_resize<Char>(min_size, max_size);
	}

private:
	friend detail::string_buffer_base;
};

class any_string_buffer : detail::string_buffer_base
{
public:
	any_string_buffer()
		: string_buffer_base(static_cast<char*>(nullptr), 0, encoding_family::none)
	{
	}

	template<
		detail::character Char,
		detail::explicit_encoding_for<Char> Encoding = detail::default_encoding_t>
	any_string_buffer(Char* const data, size_t const size, Encoding const encoding = Encoding())
		: string_buffer_base(data, size, detail::get_encoding<Char>(encoding))
	{
	}

	template<
		detail::character Char,
		size_t Size,
		detail::explicit_encoding_for<Char> Encoding = detail::default_encoding_t>
	any_string_buffer(Char(&array)[Size], Encoding const encoding = Encoding())
		: string_buffer_base(array, Size, detail::get_encoding<Char>(encoding))
	{
	}

	template<
		detail::any_mutable_string String,
		detail::explicit_container_encoding_for<String> Encoding = detail::default_encoding_t>
	any_string_buffer(String& string, Encoding const encoding = Encoding())
		: string_buffer_base(
			detail::get_mutable_range(string),
			detail::get_container_encoding<String>(encoding))
	{
	}

	//TODO: Is this overload actually needed? Document why.
	template<
		typename String,
		detail::explicit_container_encoding_for<String> Encoding = detail::default_encoding_t>
		requires detail::any_mutable_string<String const>
	any_string_buffer(String const& string, Encoding const encoding = Encoding())
		: string_buffer_base(
			detail::get_mutable_range(string),
			detail::get_container_encoding<String>(encoding))
	{
	}


	using string_buffer_base::char_type;
	using string_buffer_base::encoding;

	template<detail::character Char>
	[[nodiscard]] string_buffer<Char> buffer() const
	{
		vsm_assert(string_buffer_base::char_type() == detail::char_type_of<Char>); //PRECONDITION
		return string_buffer<Char>(*this);
	}

	[[nodiscard]] decltype(auto) visit(auto&& visitor) const
	{
		switch (string_buffer_base::char_type())
		{
		case allio::char_type::_char:
			return vsm_forward(visitor)(string_buffer<char>(*this));

		case allio::char_type::_wchar_t:
			return vsm_forward(visitor)(string_buffer<wchar_t>(*this));

		case allio::char_type::_char8_t:
			return vsm_forward(visitor)(string_buffer<char8_t>(*this));

		case allio::char_type::_char16_t:
			return vsm_forward(visitor)(string_buffer<char16_t>(*this));

		case allio::char_type::_char32_t:
			return vsm_forward(visitor)(string_buffer<char32_t>(*this));
		}

		vsm_unreachable();
	}

private:
	friend detail::string_buffer_base;
};

namespace detail {

template<vsm::character To, vsm::character From>
	requires (sizeof(To) == sizeof(From))
[[nodiscard]] string_buffer<To> reinterpret(string_buffer<From> const& string)
{
	return string_buffer<To>(string_buffer_base::get(string));
}

} // namespace detail
} // namespace allio
