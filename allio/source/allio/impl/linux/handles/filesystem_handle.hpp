#pragma once

#include <allio/fs_object.hpp>

#include <allio/impl/linux/platform_object.hpp>

#include <fcntl.h>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

struct open_parameters
{
	int flags;
	mode_t mode;

	static vsm::result<open_parameters> make(filesystem_handle::open_parameters const& args);
};

vsm::result<detail::unique_fd> create_file(
	filesystem_handle const* base, input_path_view path,
	open_parameters args);

vsm::result<detail::unique_fd> create_file(
	filesystem_handle const* base, input_path_view path,
	filesystem_handle::open_parameters const& args);

} // namespace allio::linux

#include <allio/linux/detail/undef.i>