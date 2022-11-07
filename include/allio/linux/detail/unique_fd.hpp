#pragma once

#include <allio/detail/unique_resource.hpp>
#include <allio/result.hpp>

namespace allio::detail {

struct fd_deleter
{
	static void release(int fd);

	void operator()(int const fd) const
	{
		release(fd);
	}
};
using unique_fd = unique_resource<int, fd_deleter, static_cast<int>(-1)>;

} // namespace allio::detail
