#pragma once

#include <allio/process_handle.hpp>

#include <allio/impl/linux/platform_handle.hpp>

#include <unistd.h>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

struct process_info
{
	unique_fd pid_fd;
	process_id pid;
};

vsm::result<unique_fd> open_process(process_id pid);
vsm::result<process_info> launch_process(filesystem_handle const* base, path_view path, process_handle::launch_parameters const& args);

} // namespace allio::linux

#include <allio/linux/detail/undef.i>
