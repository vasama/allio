#pragma once

#include <allio/impl/handles/fs_object.hpp>

#include <allio/impl/win32/handles/platform_object.hpp>

namespace allio::win32 {

enum class open_kind : uint8_t
{
	file,
	directory,
};

struct open_parameters
{
	open_kind kind;
	bool inheritable;
	bool multiplexable;
	file_mode mode;
	file_creation creation;
	file_sharing sharing;
	file_caching caching;
	file_flags flags;

	static open_parameters make(
		open_kind const kind,
		detail::fs_io::open_t::optional_params_type const& args);
};

#if 0
vsm::result<unique_handle_with_flags> create_file(
	HANDLE hint_handle,
	file_id_128 const& id,
	open_parameters const& args);
#endif

vsm::result<unique_handle_with_flags> create_file(
	HANDLE base_handle,
	any_path_view path,
	open_parameters const& args);

vsm::result<unique_handle_with_flags> reopen_file(
	HANDLE handle,
	open_parameters const& args);

} // namespace allio::win32
