#pragma once

#include <allio/impl/filesystem.hpp>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

using detail::platform_open_options;

vsm::result<detail::unique_handle> create_file(
	int dir_fd,
	char const* path,
	int flags,
	mode_t mode = 0);

vsm::result<detail::unique_handle> create_file(
	int dir_fd,
	char const* path,
	platform_open_options const& options);

vsm::result<detail::unique_handle> reopen_file(
	int fd,
	int flags,
	mode_t mode = 0);

vsm::result<detail::unique_handle> reopen_file(
	int fd,
	platform_open_options const& options);

vsm::result<detail::unique_handle> open_anchor_file(
	int dir_fd,
	char const* path,
	bool may_open_leaf);

vsm::result<void> link_file_at(
	int fd,
	int dir_fd,
	char const* path,
	bool replace_existing_file);

vsm::result<detail::fs_info_view> query_path_info(
	int dir_fd,
	char const* path,
	detail::fs_info_mask mask,
	detail::any_byte_buffer_ref buffer);

using detail::query_file_info;

} // namespace allio::linux

#include <allio/linux/detail/undef.i>
