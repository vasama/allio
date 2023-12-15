#pragma once

#include <allio/blocking.hpp>
#include <allio/handles/section.hpp>

namespace allio::blocking {
inline namespace section {

using section_handle = detail::blocking_handle<section_t>;

#include <allio/detail/handles/section_io.ipp>

} // inline namespace section
} // namespace allio::blocking
