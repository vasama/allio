#pragma once

#include <allio/mapped.hpp>
#include <allio/nothrow/blocking/map.hpp>

namespace allio::nothrow::blocking {
inline namespace mapping {

template<typename T>
using mapped = basic_mapped<T, map_handle>;

template<typename T>
[[nodiscard]] vsm::result<mapped<T>> map_file_as(
	detail::handle_for<file_t> auto const& file,
	auto&&... args)
{
	vsm_try(handle, map_file(file, vsm_forward(args)...));
	return vsm::result<mapped<T>>(vsm::result_value, vsm_move(handle));
}

template<typename T>
[[nodiscard]] vsm::result<mapped<T>> map_path_as(
	detail::fs_path const& path,
	auto&&... args)
{
	vsm_try(handle, map_path(path, vsm_forward(args)...));
	return vsm::result<mapped<T>>(vsm::result_value, vsm_move(handle));
}

} // inline namespace mapping
} // namespace allio::nothrow::blocking
