#pragma once

#include <allio/impl/handles/fs_object.hpp>

#include <allio/detail/unique_handle.hpp>

#include <fcntl.h>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

struct open_info
{
	int flags;
	mode_t mode;

	static vsm::result<open_info> make(detail::open_parameters const& args);
};

vsm::result<detail::unique_handle> open_file(
	int dir_fd,
	char const* path,
	open_info const& info);

vsm::result<detail::unique_handle> open_file(
	int dir_fd,
	any_path_view path,
	open_info const& info);

vsm::result<detail::unique_handle> reopen_file(
	int fd,
	open_info const& info);


inline vsm::result<detail::unique_handle> open_file(
	int dir_fd,
	char const* path,
	int flags,
	mode_t mode = 0)
{
	return open_file(dir_fd, path, { flags, mode });
}

inline vsm::result<detail::unique_handle> reopen_file(
	int fd,
	int flags,
	mode_t mode = 0)
{
	return reopen_file(fd, { flags, mode });
}

} // namespace allio::linux

#include <allio/linux/detail/undef.i>
