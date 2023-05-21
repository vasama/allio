#pragma once

#include <allio/filesystem_handle.hpp>

#include <allio/impl/win32/platform_handle.hpp>

namespace allio::win32 {

enum class open_kind
{
	file                                = 1 << 0,
	directory                           = 1 << 1,
};
vsm_flag_enum(open_kind);

vsm::result<unique_handle_with_flags> create_file(
	filesystem_handle const* hint, file_id_128 const& id,
	filesystem_handle::open_parameters const& args, open_kind kind);

vsm::result<unique_handle_with_flags> create_file(
	filesystem_handle const* base, input_path_view path,
	filesystem_handle::open_parameters const& args, open_kind kind);

vsm::result<unique_handle_with_flags> reopen_file(
	filesystem_handle const& handle,
	filesystem_handle::open_parameters const& args, open_kind kind);

} // namespace allio::win32
