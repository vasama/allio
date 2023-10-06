#pragma once

#include <allio/handles/filesystem_handle.hpp>

#include <allio/impl/win32/handles/platform_handle.hpp>

namespace allio::win32 {

using open_args = filesystem_handle::open_t::optional_params_type;

enum class open_kind
{
	file,
	directory,
};


vsm::result<unique_handle_with_flags> create_file(
	HANDLE hint_handle,
	file_id_128 const& id,
	open_kind kind,
	open_args const& args);

vsm::result<unique_handle_with_flags> create_file(
	HANDLE base_handle,
	any_path_view path,
	open_kind kind,
	open_args const& args);

vsm::result<unique_handle_with_flags> reopen_file(
	HANDLE handle,
	open_kind kind,
	open_args const& args);

} // namespace allio::win32
