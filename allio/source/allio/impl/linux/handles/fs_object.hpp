#pragma once

#include <allio/impl/handles/fs_object.hpp>

#include <allio/linux/detail/unique_fd.hpp>

#include <fcntl.h>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

struct open_info
{
	int flags;
	mode_t mode;

	static vsm::result<open_info> make(open_parameters const& args);
};

vsm::result<detail::unique_fd> open_file(
	int dir_fd,
	char const* path,
	int flags,
	mode_t mode = 0);

vsm::result<detail::unique_fd> reopen_file(
	int fd,
	int flags,
	mode_t mode = 0);

} // namespace allio::linux

#include <allio/linux/detail/undef.i>
