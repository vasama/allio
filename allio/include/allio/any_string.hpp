#pragma once

#include <allio/encoding.hpp>

#include <vsm/assert.h>
#include <vsm/box.hpp>
#include <vsm/result.hpp>
#include <vsm/utility.hpp>

#include <ranges>
#include <span>
#include <string_view>

#include <cstdint>

namespace allio {
namespace detail {

void _character_ptr(auto) = delete;
void _character_ptr(character auto const* data);

template<typename P>
concept character_ptr = requires { _character_ptr(P(nullptr)); };

template<typename String>
concept _any_string = requires(String const& string)
{
	requires character<typename String::value_type>;
	{ string.data() } -> character_ptr;
	{ string.size() } -> std::convertible_to<size_t>;
};

template<typename Range>
concept _any_string_range =
	std::ranges::contiguous_range<Range> &&
	_any_string<std::ranges::range_value_t<Range>>;

struct string_length_out_of_range_t {};

} // namespace detail


struct null_terminated_t {};
inline constexpr null_terminated_t null_terminated = {};

class any_string_view
{
	static constexpr uint32_t ctrl_bits = 32;

	static constexpr uint32_t cstr_flag = (static_cast<uint32_t>(1) << (ctrl_bits - 1));

	static constexpr uint32_t type_bits = 3;
	static constexpr uint32_t type_mask = (static_cast<uint32_t>(1) << type_bits) - 1;

	static constexpr uint32_t size_bits = ctrl_bits - type_bits - 1;
	static constexpr uint32_t size_mask = (static_cast<uint32_t>(1) << size_bits) - 1;

	template<detail::_any_string String>
	static constexpr uint32_t cstr_flag_for = cstr_flag * requires (String const& string)
	{
		{ string.c_str() } -> std::same_as<decltype(string.data())>;
	};

	template<detail::character Char>
	static constexpr uint32_t type_mask_for = static_cast<uint32_t>(detail::encoding_of<Char>) << size_bits;

	void const* m_data;
	uint32_t m_ctrl;

public:
	any_string_view()
		: any_string_view("", 0, cstr_flag)
	{
	}

	template<detail::_any_string String>
	any_string_view(String const& string)
		: any_string_view(string.data(), string.size(), cstr_flag_for<String>)
	{
	}

	template<detail::_any_string String>
	explicit any_string_view(String const& string, null_terminated_t)
		: any_string_view(string.data(), string.size(), cstr_flag)
	{
	}

	template<detail::character Char>
	any_string_view(Char const* const c_string)
		: any_string_view(c_string, std::char_traits<Char>::length(c_string), cstr_flag)
	{
	}

	template<detail::character Char>
	explicit any_string_view(Char const* const data, size_t const size)
		: any_string_view(data, size, 0)
	{
	}

	template<detail::character Char>
	explicit any_string_view(Char const* const data, size_t const size, null_terminated_t)
		: any_string_view(data, size, cstr_flag)
	{
		vsm_assert(std::char_traits<Char>::length(data) <= size);
	}


	[[nodiscard]] allio::encoding encoding() const
	{
		return static_cast<allio::encoding>((m_ctrl >> size_bits) & type_mask);
	}

	[[nodiscard]] bool empty() const
	{
		return (m_ctrl & size_mask) == 0;
	}

	[[nodiscard]] size_t size() const
	{
		return static_cast<size_t>(m_ctrl & size_mask);
	}

	template<detail::character Char>
	[[nodiscard]] Char const* data() const
	{
		vsm_assert(encoding() == detail::encoding_of<Char>); //PRECONDITION
		return static_cast<Char const*>(m_data);
	}

	[[nodiscard]] bool is_null_terminated() const
	{
		return m_ctrl & cstr_flag;
	}

	template<detail::character Char>
	[[nodiscard]] std::basic_string_view<Char> view() const
	{
		vsm_assert(encoding() == detail::encoding_of<Char>); //PRECONDITION
		return std::basic_string_view<Char>(
			static_cast<Char const*>(m_data),
			static_cast<size_t>(m_ctrl & size_mask));
	}


	[[nodiscard]] decltype(auto) visit(auto&& visitor) const
	{
		switch (encoding())
		{
		case allio::encoding::narrow_execution_encoding:
			return _visitor<char>(vsm_forward(visitor));

		case allio::encoding::wide_execution_encoding:
			return _visitor<wchar_t>(vsm_forward(visitor));

		case allio::encoding::utf8:
			return _visitor<char8_t>(vsm_forward(visitor));

		case allio::encoding::utf16:
			return _visitor<char16_t>(vsm_forward(visitor));

		case allio::encoding::utf32:
			return _visitor<char32_t>(vsm_forward(visitor));
		}

		return vsm_forward(visitor)(detail::string_length_out_of_range_t());
	}

private:
	template<detail::character Char>
	explicit any_string_view(Char const* const data, size_t const size, uint32_t const cstr_flag)
		: m_data(data)
		, m_ctrl(size > size_mask ? size_mask : static_cast<uint32_t>(size) | type_mask_for<Char> | cstr_flag)
	{
	}

	template<detail::character Char, typename Visitor>
	decltype(auto) _visitor(Visitor&& visitor) const
	{
		if constexpr (std::is_invocable_v<Visitor&&, std::basic_string_view<Char>, null_terminated_t>)
		{
			if (m_ctrl & cstr_flag)
			{
				return vsm_forward(visitor)(view<Char>(), null_terminated);
			}
		}
		return vsm_forward(visitor)(view<Char>());
	}
};


class any_string_span
{
	using get_type = any_string_view(void const* data, size_t index);

	void const* m_data;
	size_t m_size;
	get_type* m_get;

public:
	class iterator
	{
		void const* m_data;
		size_t m_index;
		get_type* m_get;

	public:
		any_string_view operator*() const
		{
			return m_get(m_data, m_index);
		}

		vsm::box<any_string_view> operator->() const
		{
			return m_get(m_data, m_index);
		}

		iterator& operator++() &
		{
			++m_index;
			return *this;
		}

		iterator operator++(int) &
		{
			iterator const it = *this;
			++m_index;
			return it;
		}

		friend bool operator==(iterator const& lhs, iterator const& rhs)
		{
			return lhs.m_index == rhs.m_index;
		}

	private:
		explicit iterator(void const* const data, size_t const index, get_type* const get)
			: m_data(data)
			, m_index(index)
			, m_get(get)
		{
		}

		friend any_string_span;
	};


	any_string_span()
		: m_data(nullptr)
		, m_size(0)
		, m_get(nullptr)
	{
	}

	template<detail::_any_string_range Range>
	any_string_span(Range const& range)
		: m_data(std::ranges::data(range))
		, m_size(std::ranges::size(range))
		, m_get(get<std::ranges::range_value_t<Range const&>>)
	{
	}

	template<detail::_any_string String>
	any_string_span(String const* const data, size_t const size)
		: m_data(data)
		, m_size(size)
		, m_get(get<String>)
	{
	}


	[[nodiscard]] bool empty() const
	{
		return m_size == 0;
	}

	[[nodiscard]] size_t size() const
	{
		return m_size;
	}


	[[nodiscard]] iterator begin() const
	{
		return iterator(m_data, 0, m_get);
	}

	[[nodiscard]] iterator end() const
	{
		return iterator(m_data, m_size, m_get);
	}

private:
	template<typename T>
	static any_string_view get(void const* const data, size_t const index)
	{
		return static_cast<T const*>(data)[index];
	}
};

} // namespace allio
