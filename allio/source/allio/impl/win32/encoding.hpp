#pragma once

#include <allio/impl/win32/error.hpp>
#include <allio/output_string_ref.hpp>

#include <vsm/result.hpp>

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


vsm::result<size_t> utf8_to_wide(std::basic_string_view<char> string, std::span<wchar_t> buffer);
vsm::result<size_t> utf8_to_wide(std::basic_string_view<char> string);

vsm::result<size_t> wide_to_utf8(std::basic_string_view<wchar_t> string, std::span<char> buffer);
vsm::result<size_t> wide_to_utf8(std::basic_string_view<wchar_t> string);

vsm::result<size_t> copy_string(std::wstring_view string, output_string_ref output);

} // namespace allio::win32
