#pragma once

#include <allio/filesystem_handle.hpp>

#include "platform_handle.hpp"

#include <fcntl.h>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

struct open_parameters
{
	int flags;
	mode_t mode;

	static result<open_parameters> make(file_parameters const& args);
};

result<unique_fd> create_file(filesystem_handle const* const base, path_view const path, file_parameters const& args);

} // namespace allio::linux

#include <allio/linux/detail/undef.i>
