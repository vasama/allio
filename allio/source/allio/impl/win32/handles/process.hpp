#pragma once

#include <allio/handles/process.hpp>

#include <allio/handles/fs_object.hpp>
#include <allio/win32/handles/process.hpp>
#include <allio/win32/detail/unique_handle.hpp>

#include <win32.h>

namespace allio::win32 {

struct process_info
{
	detail::unique_handle handle;
	process_id pid;
};

vsm::result<detail::unique_handle> open_process(detail::process_id pid);

vsm::result<process_info> launch_process(
	path_descriptor path,
	any_string_span arguments,
	any_string_span environment,
	path_descriptor const* working_directory);

vsm::result<process_exit_code> get_process_exit_code(HANDLE handle);

} // namespace allio::win32
