#pragma once

#include <allio/default_multiplexer.hpp>
#include <allio/detail/task.hpp>

namespace allio {

using detail::basic_task;

template<typename T>
using task = basic_task<T, default_multiplexer_handle>;

} // namespace allio
