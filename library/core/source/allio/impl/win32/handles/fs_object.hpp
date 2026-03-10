#pragma once

#include <allio/impl/handles/fs_object.hpp>

#include <allio/impl/win32/handles/platform_object.hpp>

namespace allio::win32 {

using detail::platform_open_options;

#if 0
vsm::result<handle_with_flags> create_file(
	HANDLE hint_handle,
	file_id_128 const& id,
	platform_open_options const& options);
#endif

vsm::result<detail::handle_with_flags> create_file(
	HANDLE base_handle,
	UNICODE_STRING path,
	platform_open_options const& options);

vsm::result<detail::handle_with_flags> reopen_file(
	HANDLE handle,
	platform_open_options const& options);

vsm::result<void> _link_file_at(
	HANDLE handle,
	HANDLE base_handle,
	std::wstring_view path,
	bool replace_existing_file);

vsm::result<void> link_file_at(
	HANDLE handle,
	HANDLE base_handle,
	any_path_view path,
	bool replace_existing_file);

} // namespace allio::win32
