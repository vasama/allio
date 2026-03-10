#pragma once

#include <allio/impl/handles/fs_object_open.hpp>

#include <fcntl.h>

namespace allio::detail {

struct platform_open_options
{
	int flags;
	mode_t mode;

	static vsm::result<platform_open_options> make(generic_open_options const& args);
};

} // namespace allio::detail
