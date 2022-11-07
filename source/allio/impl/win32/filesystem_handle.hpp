#pragma once

#include <allio/filesystem_handle.hpp>

#include <allio/impl/win32/platform_handle.hpp>

namespace allio::win32 {

enum class open_kind
{
	file = 1 << 0,
	directory = 1 << 1,
};
allio_detail_FLAG_ENUM(open_kind);

result<unique_handle_with_flags> create_file(
	filesystem_handle const* hint, file_id const& id,
	filesystem_handle::open_parameters const& args, open_kind kind);

result<unique_handle_with_flags> create_file(
	filesystem_handle const* base, input_path_view path,
	filesystem_handle::open_parameters const& args, open_kind kind);

result<unique_handle_with_flags> reopen_file(
	filesystem_handle const& handle,
	filesystem_handle::open_parameters const& args, open_kind kind);

} // namespace allio::win32
