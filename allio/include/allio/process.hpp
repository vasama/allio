#pragma once

#include <allio/default_multiplexer.hpp>
#include <allio/handles/process.hpp>

namespace allio {

allio_detail_export
template<typename Multiplexer = default_multiplexer_handle>
using process_handle = basic_process_handle<Multiplexer>;

} // namespace allio
