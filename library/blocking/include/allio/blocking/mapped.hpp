#pragma once

#include <allio/mapped.hpp>
#include <allio/blocking/map.hpp>

#include <vsm/concepts.hpp>

namespace allio::blocking {
inline namespace mapping {

template<vsm::non_ref T>
using mapped = basic_mapped<T, map_handle>;

template<vsm::non_ref T>
[[nodiscard]] mapped<T> map_file_as(
	detail::handle_for<detail::file_t> auto const& file,
	auto&&... args)
{
	return mapped<T>(map_file(file, vsm_forward(args)...));
}

template<vsm::non_ref T>
[[nodiscard]] mapped<T> map_file_as(detail::fs_path const& path, auto&&... args)
{
	return mapped<T>(map_file(path, vsm_forward(args)...));
}

} // inline namespace mapping
} // namespace allio::blocking
