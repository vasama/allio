#pragma once

#include <allio/detail/handles/process.hpp>

namespace allio {

using detail::process_id;
using detail::process_exit_code;

using detail::inherit_handles_t;
using detail::inherit_handles;

using detail::command_line_t;
using detail::command_line;

using detail::environment_t;
using detail::environment;

using detail::working_directory_t;
using detail::working_directory;

using detail::process_t;
using detail::abstract_process_handle;

namespace blocking { using namespace detail::_process_b; }
namespace async { using namespace detail::_process_a; }

namespace this_process {

process_id get_id();
blocking::process_handle const& get_handle();
vsm::result<blocking::process_handle> open();

} // namespace this_process
} // namespace allio
