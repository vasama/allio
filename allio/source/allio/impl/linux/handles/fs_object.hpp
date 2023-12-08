#pragma once

#include <allio/detail/handles/fs_object.hpp>
#include <allio/linux/detail/unique_fd.hpp>

#include <fcntl.h>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

using open_parameters = detail::fs_io::open_t::optional_params_type;

enum class open_kind
{
	file,
	directory,
};

struct open_info
{
	int flags;
	mode_t mode;
};

vsm::result<open_info> make_open_info(open_kind kind, open_parameters const& args);

vsm::result<detail::unique_fd> open_file(
	int dir_fd,
	char const* path,
	int flags,
	mode_t mode = 0);

} // namespace allio::linux

#include <allio/linux/detail/undef.i>
