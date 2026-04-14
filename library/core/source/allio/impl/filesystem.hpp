#pragma once

#include <allio/detail/filesystem.hpp>

#include <allio/detail/handles/fs_object.hpp>
#include <allio/detail/unique_handle.hpp>

namespace allio::detail {

vsm::result<unique_handle> open_anchor_file(
	platform_handle_type base_handle,
	any_path_view path,
	bool may_open_leaf);

vsm::result<void> link_file_at(
	platform_handle_type handle,
	platform_handle_type base_handle,
	any_path_view path,
	bool replace_existing_file);

} // namespace allio::detail
