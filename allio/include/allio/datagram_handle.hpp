#pragma once

#include <allio/default_multiplexer.hpp>
#include <allio/handles/datagram_handle.hpp>

namespace allio {

allio_detail_export
template<typename Multiplexer = default_multiplexer_handle>
using datagram_handle = basic_datagram_handle<Multiplexer>;

} // namespace allio
