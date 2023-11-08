#pragma once

#include <allio/default_multiplexer.hpp>
#include <allio/handles/listen_handle.hpp>

namespace allio {

allio_detail_export
template<typename Multiplexer = default_multiplexer_handle>
using listen_handle = detail::basic_handle<listen_handle_t, Multiplexer>;

} // namespace allio
