#pragma once

#include <allio/detail/handles/section_handle.hpp>

namespace allio {

using detail::section_protection_t;
using detail::section_protection;

using detail::section_t;
using detail::abstract_section_handle;

using section_handle = basic_blocking_handle<detail::_section_handle>;

namespace blocking { using namespace detail::_section_blocking; }

} // namespace allio
