#define WIN32_NLS

#include <allio/impl/win32/encoding.hpp>

#include <allio/impl/win32/error.hpp>

#include <vsm/assert.h>

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

vsm::result<size_t> win32::utf8_to_wide(std::basic_string_view<char> const string, std::span<wchar_t> const buffer)
{
	if (string.empty())
	{
		return 0;
	}

	int const size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, string.data(), string.size(), buffer.data(), buffer.size());

	if (size == 0)
	{
		return std::unexpected(get_last_error());
	}

	return static_cast<size_t>(size);
}

vsm::result<size_t> win32::utf8_to_wide(std::basic_string_view<char> const string)
{
	return utf8_to_wide(string, {});
}

vsm::result<size_t> win32::wide_to_utf8(std::basic_string_view<wchar_t> const string, std::span<char> const buffer)
{
	if (string.empty())
	{
		return 0;
	}

	int const size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, string.data(), string.size(), buffer.data(), buffer.size(), nullptr, nullptr);

	if (size == 0)
	{
		return std::unexpected(get_last_error());
	}

	return static_cast<size_t>(size);
}

vsm::result<size_t> win32::wide_to_utf8(std::basic_string_view<wchar_t> const string)
{
	return wide_to_utf8(string, {});
}


vsm::result<size_t> win32::copy_string(std::wstring_view const string, output_string_ref const output)
{
	bool const c_string = output.is_c_string();

	if (output.is_wide())
	{
		size_t const wide_size = string.size();
		size_t const wide_buffer_size = wide_size + c_string;
		vsm_try(buffer, output.resize_wide(wide_buffer_size));
		buffer[wide_buffer_size - 1] = L'\0';
		memcpy(buffer.data(), string.data(), wide_size * sizeof(wchar_t));
		return wide_size;
	}
	else
	{
		//TODO: Improve the encoding API and transcode into preallocated buffer if available.

		vsm_try(utf8_size, wide_to_utf8(string));
		size_t const utf8_buffer_size = utf8_size + c_string;
		vsm_try(buffer, output.resize_utf8(utf8_buffer_size));
		buffer[utf8_buffer_size - 1] = '\0';
		vsm_verify(wide_to_utf8(string, buffer));
		return utf8_size;
	}
}
