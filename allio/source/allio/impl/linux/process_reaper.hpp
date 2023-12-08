#pragma once

#include <vsm/result.hpp>
#include <vsm/standard.hpp>

#include <memory>

#include <allio/linux/detail/undef.i>

namespace allio::detail {

struct unix_process_reaper;

} // namespace allio::detail

namespace allio::linux {

using process_reaper = detail::unix_process_reaper;
void release_process_reaper(process_reaper* reaper);

struct process_reaper_deleter
{
	void vsm_static_operator_invoke(process_reaper* const reaper)
	{
		release_process_reaper(reaper);
	}
};
using process_reaper_ptr = std::unique_ptr<process_reaper, process_reaper_deleter>;

vsm::result<process_reaper_ptr> acquire_process_reaper();
void start_process_reaper(process_reaper* reaper, int fd);

vsm::result<int> process_reaper_wait(process_reaper* reaper, int fd);

} // namespace allio::linux

#include <allio/linux/detail/undef.i>
