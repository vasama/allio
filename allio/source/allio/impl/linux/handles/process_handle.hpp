#pragma once

#include <allio/process.hpp>

#include <allio/fs_object.hpp>
#include <allio/impl/linux/platform_object.hpp>
#include <allio/linux/detail/unique_fd.hpp>

#include <allio/linux/detail/undef.i>

namespace allio {

namespace linux {

struct process_info
{
	detail::unique_fd pid_fd;
	process_id pid;
};

vsm::result<detail::unique_fd> open_process(process_id pid);

vsm::result<process_info> launch_process(
	filesystem_handle const* base, input_path_view path,
	process_handle::launch_parameters const& args);

} // namespace linux
} // namespace allio

#include <allio/linux/detail/undef.i>
