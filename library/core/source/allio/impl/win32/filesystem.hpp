#pragma once

#include <allio/impl/filesystem.hpp>

#include <allio/detail/basic_storage.hpp>
#include <allio/impl/win32/kernel.hpp>

namespace allio::win32 {

using detail::platform_open_options;

// Requires a valid NT path (normalized - \??\ or relative).
vsm::result<detail::unique_handle> create_file(
	HANDLE base_handle,
	std::wstring_view path,
	detail::platform_open_options const& options);

vsm::result<detail::unique_handle> reopen_file(
	HANDLE handle,
	detail::platform_open_options const& options);

vsm::result<detail::unique_handle> open_anchor_file(
	HANDLE base_handle,
	std::wstring_view path,
	bool may_open_leaf);

vsm::result<void> link_file_at(
	HANDLE handle,
	HANDLE base_handle,
	std::wstring_view path,
	bool replace_existing_file);

vsm::result<std::wstring_view> query_file_name(
	detail::basic_storage_base<wchar_t>& storage,
	HANDLE handle,
	detail::path_formats formats);

} // namespace allio::win32
