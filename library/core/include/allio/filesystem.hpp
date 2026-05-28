#pragma once

#include <allio/detail/filesystem.hpp>
#include <allio/detail/handles/fs_object.hpp>

#include <concepts>

namespace allio {
namespace detail {

struct directory_t;

} // namespace detail

using detail::fs_size;
using detail::fs_clock;
using detail::fs_time_point;
using detail::fs_dev_id;
using detail::fs_ino_id;
using detail::fs_type;
using detail::fs_io_limits;
using detail::fs_info_mask;
using detail::fs_info_view;
using detail::fs_standard_info;
using detail::fs_info_buffer_ref;
using detail::fs_info_buffer;
using detail::fs_path;
using detail::is_null_device_path;

template<detail::handle_for<detail::fs_object_t> Handle>
[[nodiscard]] fs_path at(Handle const& location)
{
	vsm_assert(location); //PRECONDITION

	fs_path path;
	path.base = &location.native();
	path.path = {};
	return path;
}

template<detail::handle_for<detail::directory_t> Handle, std::convertible_to<any_path_view> Path>
[[nodiscard]] fs_path at(Handle const& location, Path const& relative_path)
{
	vsm_assert(location); //PRECONDITION

	fs_path path;
	path.base = &location.native();
	path.path = relative_path;
	return path;
}

} // namespace allio
