#pragma once

#include <allio/detail/handles/map.hpp>

namespace allio {

using detail::initial_commit;
using detail::map_address;

using detail::map_t;
using detail::abstract_map_handle;

namespace blocking { using namespace detail::_map_b; }

} // namespace allio
