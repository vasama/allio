#pragma once

#include <allio/blocking.hpp>
#include <allio/handles/map.hpp>

namespace allio::blocking {
inline namespace map {

using map_handle = detail::blocking_handle<map_t>;

#include <allio/detail/handles/map_io.ipp>

} // inline namespace map
} // namespace allio::blocking
