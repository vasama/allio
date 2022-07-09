#pragma once

#include <allio/detail/unique_resource.hpp>

namespace allio::detail {

struct fd_deleter
{
	static void release(int fd);

	void operator()(int const fd) const
	{
		release(fd);
	}
};

using unique_fd = unique_resource<int, fd_deleter, -1>;

} // namespace allio::detail
