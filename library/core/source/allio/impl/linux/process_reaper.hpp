#pragma once

#include <vsm/result.hpp>
#include <vsm/standard.hpp>

#include <memory>
#include <optional>

#include <allio/linux/detail/undef.i>

namespace allio::detail {

struct unix_process_reaper;

} // namespace allio::detail

namespace allio::linux {

using process_reaper = detail::unix_process_reaper;
void release_process_reaper(process_reaper* reaper);

struct process_reaper_deleter
{
	vsm_static_operator void operator()(process_reaper* const reaper) vsm_static_operator_const
	{
		release_process_reaper(reaper);
	}
};
using process_reaper_ptr = std::unique_ptr<process_reaper, process_reaper_deleter>;

vsm::result<process_reaper_ptr> acquire_process_reaper();

/// @note This function takes ownership of the file descriptor.
void start_process_reaper(process_reaper* reaper, int fd);

/// @note The file descriptor must match the one previously passed to @ref start_process_reaper.
vsm::result<std::optional<int>> process_reaper_wait(process_reaper* reaper, int fd);

} // namespace allio::linux

#include <allio/linux/detail/undef.i>
