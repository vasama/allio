#pragma once

#include <system_error>

#include <win32.h>

namespace allio::win32 {

inline std::error_code get_last_error_code()
{
	return std::error_code(GetLastError(), std::system_category());
}

} // namespace allio::win32
