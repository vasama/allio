#pragma once

#include <allio/blocking.hpp>
#include <allio/handles/pipe.hpp>

namespace allio::blocking {
inline namespace pipe {

using pipe_handle = detail::blocking_handle<pipe_t>;
using pipe_handle_pair = detail::basic_pipe_pair<pipe_handle>;

#include <allio/detail/handles/pipe_io.ipp>

} // inline namespace pipe
} // namespace allio::blocking
