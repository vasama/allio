#pragma once

#include <system_error>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

inline std::error_code get_last_error_code()
{
	return std::error_code(errno, std::system_category());
}

} // namespace allio::linux

#include <allio/linux/detail/undef.i>
