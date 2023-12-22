#pragma once

#include <allio/handles/process.hpp>

#include <allio/detail/unique_handle.hpp>
#include <allio/handles/fs_object.hpp>
#include <allio/impl/win32/handles/platform_object.hpp>

namespace allio::win32 {

struct process_info
{
	detail::unique_handle handle;
	DWORD id;
};

vsm::result<detail::unique_handle> open_process(DWORD id);
vsm::result<process_exit_code> get_process_exit_code(HANDLE handle);

} // namespace allio::win32
