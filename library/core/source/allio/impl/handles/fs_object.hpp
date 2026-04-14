#pragma once

#include <allio/detail/handles/fs_object.hpp>

#include <allio/impl/handles/platform_object.hpp>

#include vsm_pp_include(allio/impl/vsm_os/open_options.hpp)

namespace allio::detail {

using fs_open_params_type = fs_io::open_t::params_type;
//using open_parameters [[deprecated]] = fs_open_params_type;

enum class open_kind : uint8_t
{
	path,
	file,
	directory,
};

struct generic_open_options
{
	open_kind kind;
	file_mode mode;
	// file_options options;
	file_opening opening;
	file_sharing sharing;
	file_caching caching;
	open_options special;
	io_flags flags;
};

vsm::result<platform_open_options> make_platform_open_options(generic_open_options const& args);


#if 0
// Opens the specified path for use as a path base handle.
vsm::result<unique_handle> open_path_base(platform_handle_type base, any_path_view path);
#endif

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

vsm::result<handle_flags> configure_file(
	platform_handle_type handle,
	platform_open_options const& options);

vsm::result<void> open_fs_object(
	native_handle<fs_object_t>& h,
	open_kind kind,
	fs_open_params_type const& args);

} // namespace allio::detail
