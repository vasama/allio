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

vsm::result<void> link_file_at(
	platform_handle_type handle,
	platform_handle_type base_handle,
	any_path_view path,
	bool replace_existing_file);

} // namespace allio::detail
