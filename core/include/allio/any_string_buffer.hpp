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
	mutable_buffer<String> &&
	character<typename String::value_type>;

template<typename String, typename Char>
concept _mutable_string_of =
	_any_mutable_string<String> &&
	std::is_same_v<typename String::value_type, Char>;


struct _string_buffer
{
	static constexpr uint32_t ctrl_bits = 32;

	static constexpr uint32_t type_bits = 4;
	static constexpr uint32_t type_mask = (static_cast<uint32_t>(1) << type_bits) - 1;

	static constexpr uint32_t size_bits = ctrl_bits - type_bits - 1;
	static constexpr uint32_t size_mask = (static_cast<uint32_t>(1) << size_bits) - 1;

	template<detail::character Char>
	static constexpr uint32_t type_mask_for = static_cast<uint32_t>(detail::encoding_of<Char>) << size_bits;

	struct buffer
	{
		void* data;
		size_t size;
	};

	using resize_type = vsm::result<buffer>(_string_buffer const& self, size_t min_size, size_t max_size);

	void* m_data;
	uint32_t m_ctrl;
	resize_type* m_resize;

	template<character Char>
	explicit _string_buffer(Char* const data, size_t const size)
		: m_data(data)
		, m_ctrl(static_cast<uint32_t>(size < size_mask ? size : size_mask) | type_mask_for<Char>)
		, m_resize(_resize_span<Char>)
	{
	}
	
	template<resizable_buffer Container>
	explicit _string_buffer(Container& container)
		: m_data(&container)
		, m_ctrl(type_mask_for<typename Container::value_type>)
		, m_resize(_resize_container<Container>)
	{
	}

	[[nodiscard]] allio::encoding encoding() const
	{
		return static_cast<allio::encoding>((m_ctrl >> size_bits) & type_mask);
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
		_string_buffer const& self,
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
		_string_buffer const& self,
		size_t const min_size,
		size_t const max_size)
	{
		auto& container = *static_cast<Container*>(self.m_data);
		vsm_try_discard(resize_buffer(container, min_size, max_size));
		return buffer{ container.data(), container.size() };
	}

};

} // namespace detail

template<detail::character Char>
class string_buffer : detail::_string_buffer
{
public:
	string_buffer()
		: _string_buffer(static_cast<Char*>(nullptr), 0)
	{
	}

	explicit string_buffer(detail::_string_buffer const& buffer)
		: _string_buffer(buffer)
	{
		vsm_assert(buffer.encoding() == detail::encoding_of<Char>); //PRECONDITION
	}

	string_buffer(Char* const data, size_t const size)
		: _string_buffer(data, size)
	{
	}

	template<size_t Size>
	string_buffer(Char(&array)[Size])
		: _string_buffer(array, Size)
	{
	}

	template<detail::_mutable_string_of<Char> String>
	string_buffer(String& string)
		: _string_buffer(string.data(), string.size())
	{
	}

	template<detail::_mutable_string_of<Char> String>
		requires detail::_mutable_string_of<String const, Char>
	string_buffer(String const& string)
		: _string_buffer(string.data(), string.size())
	{
	}

	template<detail::_mutable_string_of<Char> String>
		requires detail::resizable_buffer<String>
	string_buffer(String& string)
		: _string_buffer(string)
	{
	}


	[[nodiscard]] vsm::result<std::span<Char>> resize(size_t const size) const
	{
		return _string_buffer::_resize<Char>(size, size);
	}

	[[nodiscard]] vsm::result<std::span<Char>> resize(size_t const min_size, size_t const max_size) const
	{
		vsm_assert(min_size <= max_size); //PRECONDITION
		return _string_buffer::_resize<Char>(min_size, max_size);
	}
};

class any_string_buffer : detail::_string_buffer
{
public:
	any_string_buffer()
		: _string_buffer(static_cast<char*>(nullptr), 0)
	{
	}

	template<detail::character Char>
	any_string_buffer(Char* const data, size_t const size)
		: _string_buffer(data, size)
	{
	}

	template<detail::character Char, size_t Size>
	any_string_buffer(Char(&array)[Size])
		: _string_buffer(array, Size)
	{
	}

	template<detail::_any_mutable_string String>
	any_string_buffer(String& string)
		: _string_buffer(string.data(), string.size())
	{
	}

	template<detail::_any_mutable_string String>
		requires detail::_any_mutable_string<String const>
	any_string_buffer(String const& string)
		: _string_buffer(string.data(), string.size())
	{
	}

	template<detail::_any_mutable_string String>
		requires detail::resizable_buffer<String>
	any_string_buffer(String& string)
		: _string_buffer(string)
	{
	}


	using _string_buffer::encoding;

	template<detail::character Char>
	[[nodiscard]] string_buffer<Char> buffer() const
	{
		vsm_assert(encoding() == detail::encoding_of<Char>); //PRECONDITION
		return string_buffer<Char>(*this);
	}

	[[nodiscard]] decltype(auto) visit(auto&& visitor) const
	{
		switch (encoding())
		{
		case allio::encoding::narrow_execution_encoding:
			return vsm_forward(visitor)(string_buffer<char>(*this));

		case allio::encoding::wide_execution_encoding:
			return vsm_forward(visitor)(string_buffer<wchar_t>(*this));

		case allio::encoding::utf8:
			return vsm_forward(visitor)(string_buffer<char8_t>(*this));

		case allio::encoding::utf16:
			return vsm_forward(visitor)(string_buffer<char16_t>(*this));

		case allio::encoding::utf32:
			return vsm_forward(visitor)(string_buffer<char32_t>(*this));
		}

		vsm_unreachable();
	}
};

} // namespace allio
