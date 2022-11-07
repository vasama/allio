#pragma once

#include <allio/detail/assert.hpp>
#include <allio/detail/platform.hpp>

#include <string_view>

namespace allio {

class input_string_view
{
	// String size exceeded maximum.
	static constexpr size_t size_flag = ~(static_cast<size_t>(-1) >> 1);

	// String is null-terminated.
	static constexpr size_t null_flag = size_flag >> 1;

#if allio_detail_PLATFORM_WIDE
	static constexpr size_t wide_flag = size_flag >> 2;
#else
	static constexpr size_t wide_flag = 0;
#endif

	static constexpr size_t size_mask = static_cast<size_t>(-1) >> 3;

	void const* m_data;
	size_t m_size;

public:
	constexpr input_string_view(char const* const c_string)
		: input_string_view(c_string, std::char_traits<char>::length(c_string), true)
	{
	}

	constexpr input_string_view(std::basic_string_view<char> const string, bool const is_c_string = false)
		: input_string_view(string.data(), string.size(), is_c_string)
	{
	}

	explicit constexpr input_string_view(char const* const data, size_t const size, bool const is_c_string = false)
		: m_data(data)
		, m_size(size <= size_mask
			? size | (null_flag & -static_cast<int>(is_c_string))
			: size_flag)
	{
	}


	[[nodiscard]] constexpr bool is_too_long() const
	{
		return (m_size & size_flag) != 0;
	}

	[[nodiscard]] constexpr bool empty() const
	{
		return (m_size & size_mask) == 0;
	}

	[[nodiscard]] constexpr size_t size() const
	{
		return m_size & size_mask;
	}

	[[nodiscard]] constexpr bool is_c_string() const
	{
		return (m_size & null_flag) != 0;
	}


	[[nodiscard]] constexpr bool is_utf8() const
	{
		return (m_size & wide_flag) == 0;
	}

	[[nodiscard]] constexpr std::string_view utf8_view() const
	{
		allio_ASSERT(is_utf8());
		return { static_cast<char const*>(m_data), m_size & size_mask };
	}

	[[nodiscard]] constexpr char const* utf8_c_string() const
	{
		allio_ASSERT(is_utf8() && is_c_string());
		return static_cast<char const*>(m_data);
	}


#if allio_detail_PLATFORM_WIDE
	constexpr input_string_view(wchar_t const* const c_string)
		: input_string_view(c_string, std::char_traits<wchar_t>::length(c_string), true)
	{
	}

	constexpr input_string_view(std::basic_string_view<wchar_t> const string, bool const is_c_string = false)
		: input_string_view(string.data(), string.size(), is_c_string)
	{
	}

	explicit constexpr input_string_view(wchar_t const* const data, size_t const size, bool const is_c_string)
		: m_data(data)
		, m_size(size <= size_mask
			? size | (null_flag & -static_cast<int>(is_c_string)) | wide_flag
			: size_flag)
	{
	}


	[[nodiscard]] constexpr bool is_wide() const
	{
		return (m_size & wide_flag) != 0;
	}

	[[nodiscard]] constexpr std::wstring_view wide_view() const
	{
		allio_ASSERT(is_wide());
		return { static_cast<wchar_t const*>(m_data), m_size & size_mask };
	}

	[[nodiscard]] constexpr wchar_t const* wide_c_string() const
	{
		allio_ASSERT(is_wide() && is_c_string());
		return static_cast<wchar_t const*>(m_data);
	}
#endif
};

} // namespace allio
