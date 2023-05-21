#pragma once

#include <vsm/result.hpp>

#include <win32.h>

namespace allio::win32 {

enum class system_error : DWORD
{
	none = 0
};

inline system_error get_last_error()
{
	return static_cast<system_error>(GetLastError());
}

inline std::error_code make_error_code(system_error const error)
{
	return std::error_code(static_cast<int>(error), std::system_category());
}

template<typename T>
using system_result = vsm::result<T, system_error>;

} // namespace allio::win32

template<>
struct std::is_error_code_enum<allio::win32::system_error>
{
	static constexpr bool value = true;
};
