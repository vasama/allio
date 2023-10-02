#pragma once

#include <vsm/result.hpp>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

enum class system_error : int
{
	none = 0
};

inline system_error get_last_error()
{
	return static_cast<system_error>(errno);
}

inline std::error_code make_error_code(system_error const error)
{
	return std::error_code(static_cast<int>(error), std::system_category());
}

template<typename T>
using system_result = vsm::result<T, system_error>;

} // namespace allio::linux

template<>
struct std::is_error_code_enum<allio::linux::system_error>
{
	static constexpr bool value = true;
};

#include <allio/linux/detail/undef.i>
