#pragma once

#include <allio/detail/filesystem.hpp>
#include <allio/detail/handles/fs_object.hpp>

#include <concepts>

namespace allio {

using detail::fs_size;
using detail::fs_clock;
using detail::fs_time_point;
using detail::fs_device_id;
using detail::fs_node_id;
using detail::fs_permissions;
using detail::fs_entry_type;
using detail::fs_entry_info;
using detail::fs_path;
using detail::get_fs_entry_info;

template<detail::handle Handle, std::convertible_to<any_path_view> Path>
[[nodiscard]] fs_path at(Handle const& location, Path const& path)
	requires std::derived_from<typename Handle::object_type, detail::fs_object_t>
{
	vsm_assert(location); //PRECONDITION
	return fs_path(location.native().platform_handle, path);
}

} // namespace allio
