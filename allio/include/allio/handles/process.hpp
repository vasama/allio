#pragma once

#include <allio/detail/handles/process.hpp>

namespace allio {

using detail::process_id;
using detail::process_exit_code;

using detail::launch_detached;
using detail::inherit_handles;
using detail::command_line;
using detail::environment;
using detail::working_directory;
using detail::std_input;
using detail::std_output;
using detail::std_error;
using detail::wait_on_close;
using detail::with_exit_code;

using detail::process_t;
using detail::abstract_process_handle;

namespace this_process {

process_id get_id();
blocking::process_handle const& get_handle();
vsm::result<blocking::process_handle> open();

} // namespace this_process
} // namespace allio
