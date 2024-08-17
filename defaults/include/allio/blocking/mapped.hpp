#pragma once

#include <allio/mapped.hpp>
#include <allio/blocking/map.hpp>

namespace allio::blocking {
inline namespace mapping {

template<typename T>
using mapped = basic_mapped<T, map_handle>;

template<typename T>
[[nodiscard]] mapped<T> map_file_as(
	detail::handle_for<file_t> auto const& file,
	auto&&... args)
{
	return mapped<T>(map_file(file, vsm_forward(args)...));
}

//TODO: * If T is const, default to read only.
//      * Force open only mode. Don't allow creating files using this API.
template<typename T>
[[nodiscard]] mapped<T> map_path_as(
	detail::fs_path const& path,
	auto&&... args)
{
	return mapped<T>(map_path(path, vsm_forward(args)...));
}

} // inline namespace mapping
} // namespace allio::blocking
