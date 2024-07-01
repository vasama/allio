#pragma once

#include <allio/impl/handles/fs_object.hpp>

#include <allio/impl/win32/handles/platform_object.hpp>

namespace allio::win32 {

struct open_info
{
	ACCESS_MASK desired_access;
	ULONG attributes;
	ULONG share_access;
	ULONG create_disposition;
	ULONG create_options;

	static vsm::result<open_info> make(detail::open_parameters const& args);
};

#if 0
vsm::result<handle_with_flags> create_file(
	HANDLE hint_handle,
	file_id_128 const& id,
	open_info const& info);
#endif

vsm::result<detail::handle_with_flags> create_file(
	HANDLE base_handle,
	any_path_view path,
	open_info const& info);

vsm::result<detail::handle_with_flags> reopen_file(
	HANDLE handle,
	open_info const& info);

} // namespace allio::win32
