#define WIN32_NLS

#include <allio/impl/win32/encoding.hpp>

#include <allio/detail/assert.hpp>
#include <allio/impl/win32/error.hpp>

#include <win32.h>

using namespace allio;
using namespace allio::win32;

#if 0
encoding_result utf8_to_wide_2(std::basic_string_view<char> const string, std::span<wchar_t> const buffer)
{
	if (string.empty())
	{
		return {};
	}

	int const size = MultiByteToWideChar(
		CP_UTF8, MB_ERR_INVALID_CHARS,
		string.data(), string.size(),
		buffer.data(), buffer.size());

	if (size == 0)
	{
		return
		{
			.error = get_last_error(),
		};
	}

	return
	{
		.consumed = string.size(),
		.produced = static_cast<size_t>(size),
	};
}

encoding_result wide_to_utf8_2(std::basic_string_view<wchar_t> const buffer, std::span<char> const string)
{
	if (string.empty())
	{
		return {};
	}

	int const size = WideCharToMultiByte(
		CP_UTF8, WC_ERR_INVALID_CHARS,
		string.data(), string.size(),
		buffer.data(), buffer.size(),
		nullptr, nullptr);

	if (size == 0)
	{
		return
		{
			.error = get_last_error(),
		};
	}

	return
	{
		.consumed = string.size(),
		.produced = static_cast<size_t>(size),
	};
}
#endif

result<size_t> win32::utf8_to_wide(std::basic_string_view<char> const string, std::span<wchar_t> const buffer)
{
	if (string.empty())
	{
		return 0;
	}

	int const size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, string.data(), string.size(), buffer.data(), buffer.size());

	if (size == 0)
	{
		return allio_ERROR(get_last_error());
	}

	return static_cast<size_t>(size);
}

result<size_t> win32::utf8_to_wide(std::basic_string_view<char> const string)
{
	return utf8_to_wide(string, {});
}

result<size_t> win32::wide_to_utf8(std::basic_string_view<wchar_t> const string, std::span<char> const buffer)
{
	if (string.empty())
	{
		return 0;
	}

	int const size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, string.data(), string.size(), buffer.data(), buffer.size(), nullptr, nullptr);

	if (size == 0)
	{
		return allio_ERROR(get_last_error());
	}

	return static_cast<size_t>(size);
}

result<size_t> win32::wide_to_utf8(std::basic_string_view<wchar_t> const string)
{
	return wide_to_utf8(string, {});
}
