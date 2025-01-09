#pragma once

#include <allio/mapped.hpp>
#include <allio/nothrow/blocking/map.hpp>

#include <vsm/concepts.hpp>

namespace allio::nothrow::blocking {
inline namespace mapping {

template<vsm::non_ref T>
using mapped = basic_mapped<T, map_handle>;

template<vsm::non_ref T>
[[nodiscard]] vsm::result<mapped<T>> map_file_as(
	detail::handle_for<detail::file_t> auto const& file,
	auto&&... args)
{
	vsm_try(map, map_file(file, vsm_forward(args)...));
	return vsm::result<mapped<T>>(vsm::result_value, vsm_move(map));
}

//TODO: * If T is const, default to read only.
//      * Force open only mode. Don't allow creating files using this API.
template<vsm::non_ref T>
[[nodiscard]] vsm::result<mapped<T>> map_file_as(
	detail::fs_path const& path,
	auto&&... args)
{
	vsm_try(map, map_file(path, vsm_forward(args)...));
	return vsm::result<mapped<T>>(vsm::result_value, vsm_move(map));
}

} // inline namespace mapping
} // namespace allio::nothrow::blocking
