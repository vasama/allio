#pragma once

#include <allio/detail/memory.hpp>

namespace allio {

using detail::protection;
using detail::page_level;

using detail::get_page_level;
using detail::get_page_size;

using detail::get_default_page_level;
using detail::get_supported_page_levels;

using detail::get_allocation_granularity;

} // namespace allio
