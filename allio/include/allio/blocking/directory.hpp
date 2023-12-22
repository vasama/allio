#pragma once

#include <allio/blocking.hpp>
#include <allio/handles/directory.hpp>

namespace allio::blocking {
inline namespace directories {

using directory_handle = traits_type::handle<directory_t>;

[[nodiscard]] directory_handle open_directory(detail::fs_path const& path, auto&&... args)
{
	return detail::open_directory<traits_type>(path, vsm_forward(args)...);
}

[[nodiscard]] directory_handle open_temp_directory(auto&&... args)
{
	return detail::open_temp_directory<traits_type>(vsm_forward(args)...);
}

[[nodiscard]] directory_handle open_unique_directory(auto&&... args)
{
	return detail::open_unique_directory<traits_type>(vsm_forward(args)...);
}

} // inline namespace directories
} // namespace allio::blocking
