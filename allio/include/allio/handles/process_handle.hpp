#pragma once

#include <allio/detail/handles/process_handle.hpp>

namespace allio {

using detail::process_id;
using detail::process_exit_code;

using detail::command_line_t;
using detail::command_line;

using detail::environment_t;
using detail::environment;

using detail::working_directory_t;
using detail::working_directory;

using detail::process_handle_t;
using detail::basic_process_handle;

using detail::open_process;
using detail::launch_process;

namespace this_process {

using detail::_this_process::get_handle;
using detail::_this_process::open;

} // namespace this_process
} // namespace allio
