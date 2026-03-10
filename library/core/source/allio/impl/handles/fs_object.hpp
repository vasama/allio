#pragma once

#include <allio/detail/handles/fs_object.hpp>

#include <allio/impl/handles/platform_object.hpp>

#include vsm_pp_include(allio/impl/vsm_os/handles/fs_object_open.hpp)

namespace allio::detail {

vsm::result<unique_handle> open_path_base(platform_handle_type base, any_path_view path);

vsm::result<handle_with_flags> open_file(
	platform_handle_type base,
	any_path_view path,
	platform_open_options const& options);

vsm::result<handle_with_flags> open_unique_file(
	platform_handle_type base,
	platform_open_options const& options);

vsm::result<handle_with_flags> open_anonymous_file(
	platform_handle_type base,
	platform_open_options const& options);

vsm::result<void> open_fs_object(
	native_handle<fs_object_t>& h,
	open_kind kind,
	fs_open_params_type const& args);

} // namespace allio::detail
