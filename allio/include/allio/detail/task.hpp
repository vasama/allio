#pragma once

#include <exec/task.hpp>

namespace allio::detail {

template<typename T>
using task = exec::task<T>;

} // namespace allio::detail
