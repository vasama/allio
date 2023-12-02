#pragma once

#include <allio/detail/handles/pipe.hpp>

namespace allio {

using detail::pipe_t;
using detail::abstract_pipe_handle;

namespace blocking { using namespace detail::_pipe_b; }

} // namespace allio
