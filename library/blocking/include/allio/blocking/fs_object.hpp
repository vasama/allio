#pragma once

#include <allio/blocking/traits.hpp>
#include <allio/handles/fs_object.hpp>

namespace allio::blocking {
inline namespace fs_object {

template<detail::handle_for<fs_object_t> Handle>
void link_at(Handle const& handle, fs_path const& path, auto&&... args)
{
	return detail::_link_at<traits_type>(handle, path, vsm_forward(args)...);
}

} // inline namespace fs_object
} // namespace allio::blocking
