#pragma once

#include <allio/blocking.hpp>
#include <allio/handles/section.hpp>

namespace allio::blocking {
inline namespace section {

using section_handle = traits_type::handle<section_t>;

[[nodiscard]] section_handle create_section(detail::fs_size const max_size, auto&&... args)
{
	return detail::create_section<traits_type>(max_size, vsm_forward(args)...);
}

[[nodiscard]] section_handle create_section(
	detail::handle_for<detail::file_t> auto const& file,
	detail::fs_size const max_size,
	auto&&... args)
{
	return detail::create_section<traits_type>(file, max_size, vsm_forward(args)...);
}

} // inline namespace section
} // namespace allio::blocking
