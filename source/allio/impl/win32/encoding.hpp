#pragma once

#include <allio/impl/win32/error.hpp>
#include <allio/result.hpp>

#include <span>
#include <string_view>

namespace allio::win32 {

#if 0
struct encoding_result
{
	system_error error;
	size_t consumed;
	size_t produced;
};

encoding_result utf8_to_wide_2(std::basic_string_view<char> string, std::span<wchar_t> buffer);
encoding_result wide_to_utf8_2(std::basic_string_view<wchar_t> string, std::span<char> buffer);
#endif


result<size_t> utf8_to_wide(std::basic_string_view<char> string, std::span<wchar_t> buffer);
result<size_t> utf8_to_wide(std::basic_string_view<char> string);

result<size_t> wide_to_utf8(std::basic_string_view<wchar_t> string, std::span<char> buffer);
result<size_t> wide_to_utf8(std::basic_string_view<wchar_t> string);

} // namespace allio::win32
