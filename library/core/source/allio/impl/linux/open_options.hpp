#pragma once

#include <fcntl.h>

namespace allio::detail {

struct platform_open_options
{
	int flags;
	mode_t mode;
};

} // namespace allio::detail
