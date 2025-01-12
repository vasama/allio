#pragma once

#include <allio/detail/deadline.hpp>

#include <vsm/result.hpp>

#include <optional>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

vsm::result<std::optional<int>> wait_process(int fd, bool reap, detail::deadline deadline);

} // namespace allio::linux

#include <allio/linux/detail/undef.i>
