#pragma once

#include <linux/version.h>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

int get_kernel_version();

} // namespace allio::linux

#include <allio/linux/detail/undef.i>
