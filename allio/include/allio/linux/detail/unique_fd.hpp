#pragma once

#include <vsm/assert.h>
#include <vsm/result.hpp>
#include <vsm/standard.hpp>
#include <vsm/unique_resource.hpp>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

vsm::result<void> close_fd(int fd) noexcept;

struct fd_deleter
{
	void vsm_static_operator_invoke(int const fd) noexcept
	{
		vsm_verify(close_fd(fd));
	}
};
using unique_fd = vsm::unique_resource<int, fd_deleter, static_cast<int>(-1)>;

} // namespace allio::linux

#include <allio/linux/detail/undef.i>
