#pragma once

#include <allio/handles/process.hpp>

namespace allio::blocking {
inline namespace process {

using process_handle = detail::blocking_handle<process_t>;

#include <allio/detail/handles/process_io.ipp>

} // inline namespace process
} // namespace allio::blocking
