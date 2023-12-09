#pragma once

#include <allio/detail/handles/section.hpp>

namespace allio {

using detail::section_t;
using detail::abstract_section_handle;

namespace blocking { using namespace detail::_section_b; }

} // namespace allio
