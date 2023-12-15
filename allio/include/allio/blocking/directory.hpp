#pragma once

#include <allio/blocking.hpp>
#include <allio/handles/directory.hpp>

namespace allio::blocking {
inline namespace directory {

using directory_handle = detail::blocking_handle<directory_t>;

#include <allio/detail/handles/directory_io.ipp>

} // inline namespace directory
} // namespace allio::blocking
