#pragma once

#include <allio/handles/file.hpp>
#include <allio/nothrow/blocking.hpp>

namespace allio::nothrow::blocking {
inline namespace files {

using file_handle = traits_type::handle<file_t>;

[[nodiscard]] vsm::result<file_handle> open_file(detail::fs_path const& path, auto&&... args)
{
	return detail::open_file<traits_type>(path, vsm_forward(args)...);
}

[[nodiscard]] vsm::result<file_handle> open_temp_file(auto&&... args)
{
	return detail::open_temp_file<traits_type>(vsm_forward(args)...);
}

[[nodiscard]] vsm::result<file_handle> open_unique_file(
	detail::handle_for<fs_object_t> auto const& base,
	auto&&... args)
{
	return detail::open_unique_file<traits_type>(base, vsm_forward(args)...);
}

[[nodiscard]] vsm::result<file_handle> open_anonymous_file(
	detail::handle_for<fs_object_t> auto const& base,
	auto&&... args)
{
	return detail::open_anonymous_file<traits_type>(base, vsm_forward(args)...);
}

[[nodiscard]] vsm::result<file_handle> open_null_device(auto&&... args)
{
	return detail::open_null_device<traits_type>(vsm_forward(args)...);
}

} // inline namespace files
} // namespace allio::nothrow::blocking
