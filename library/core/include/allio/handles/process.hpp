#pragma once

#include <allio/detail/handles/process.hpp>

namespace allio {

using detail::process_id;
using detail::process_exit_code;

using detail::inherit_handles;
using detail::process_arguments;
using detail::process_environment;
using detail::working_directory;
using detail::redirect_stdin;
using detail::redirect_stdout;
using detail::redirect_stderr;
using detail::wait_on_close;
using detail::with_exit_code;

using detail::process_t;

} // namespace allio
