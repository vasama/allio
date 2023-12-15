#pragma once

#include <allio/blocking.hpp>
#include <allio/handles/file.hpp>

namespace allio::blocking {
inline namespace file {

using file_handle = detail::blocking_handle<file_t>;

#include <allio/detail/handles/file_io.ipp>

} // inline namespace file
} // namespace allio::blocking
