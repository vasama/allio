#pragma once

#include <allio/blocking/traits.hpp>
#include <allio/handles/map.hpp>

namespace allio::blocking {
inline namespace mapping {

using map_handle = traits_type::handle<map_t>;

[[nodiscard]] map_handle map_memory(
	size_t const size,
	auto&&... args)
{
	return detail::map_memory<traits_type>(size, vsm_forward(args)...);
}

[[nodiscard]] map_handle map_section(
	detail::handle_for<section_t> auto const& section,
	detail::fs_size const offset,
	size_t const size,
	auto&&... args)
{
	return detail::map_section<traits_type>(section, offset, size, vsm_forward(args)...);
}

[[nodiscard]] map_handle map_file(
	detail::handle_for<detail::file_t> auto const& file,
	auto&&... args)
{
	return detail::map_file<traits_type>(file, vsm_forward(args)...);
}

[[nodiscard]] map_handle map_file(
	detail::fs_path const& path,
	auto&&... args)
{
	return detail::map_file<traits_type>(path, vsm_forward(args)...);
}

} // inline namespace mapping
} // namespace allio::blocking
