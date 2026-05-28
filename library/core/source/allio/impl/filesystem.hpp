#pragma once

#include <allio/detail/filesystem.hpp>

#include <allio/detail/handles/fs_object.hpp>
#include <allio/detail/unique_handle.hpp>

#include vsm_pp_include(allio/impl/vsm_os/open_options.hpp)

namespace allio::detail {

vsm::result<unique_handle> open_anchor_file(
	platform_handle_type base_handle,
	any_path_view path,
	bool may_open_leaf);

vsm::result<unique_handle> open_file(
	platform_handle_type base,
	any_path_view path,
	platform_open_options const& options);

vsm::result<unique_handle> open_unique_file(
	platform_handle_type base,
	platform_open_options const& options);

vsm::result<unique_handle> open_anonymous_file(
	platform_handle_type base,
	platform_open_options const& options);

vsm::result<fs_info_view> query_path_info(
	platform_handle_type base_handle,
	any_path_view path,
	fs_info_mask mask,
	any_byte_buffer_ref buffer);

vsm::result<fs_info_view> query_file_info(
	platform_handle_type handle,
	fs_info_mask mask,
	any_byte_buffer_ref buffer);

vsm::result<void> link_file_at(
	platform_handle_type handle,
	platform_handle_type base_handle,
	any_path_view path,
	bool replace_existing_file);

} // namespace allio::detail
