#pragma once

#include <allio/default_multiplexer.hpp>
#include <allio/senders/process.hpp>

namespace allio {

using namespace senders::process;

using process_handle = basic_process_handle<default_multiplexer_handle>;

} // namespace allio
