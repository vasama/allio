#pragma once

#include <allio/handles/directory.hpp>
#include <allio/nothrow/blocking.hpp>

namespace allio::nothrow::blocking {
inline namespace directories {

using directory_handle = traits_type::handle<directory_t>;

[[nodiscard]] vsm::result<directory_handle> open_directory(
	detail::fs_path const& path,
	auto&&... args)
{
	return detail::open_directory<traits_type>(path, vsm_forward(args)...);
}

[[nodiscard]] vsm::result<directory_handle> open_temp_directory(auto&&... args)
{
	return detail::open_temp_directory<traits_type>(vsm_forward(args)...);
}

[[nodiscard]] vsm::result<directory_handle> open_unique_directory(auto&&... args)
{
	return detail::open_unique_directory<traits_type>(vsm_forward(args)...);
}

} // inline namespace directories

namespace this_process {

[[nodiscard]] vsm::result<size_t> get_current_directory(any_path_buffer const buffer)
{
	return detail::get_current_directory<traits_type>(buffer);
}

template<typename Path = path>
[[nodiscard]] vsm::result<Path> get_current_directory()
{
	return detail::get_current_directory<traits_type, Path>();
}

[[nodiscard]] vsm::result<void> set_current_directory(fs_path const& path)
{
	return detail::set_current_directory<traits_type>(path);
}

[[nodiscard]] vsm::result<directory_handle> open_current_directory()
{
	return detail::open_current_directory<traits_type>();
}

} // namespace this_process
} // namespace allio::nothrow::blocking
