#include <allio/detail/platform.hpp>

#include <allio/impl/linux/error.hpp>

#include <unistd.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

void detail::close_platform_handle(int const fd) noexcept
{
	static_assert(vsm_os_linux, "Check close behaviour on EINTR");

	if (::close(fd) == -1)
	{
		unrecoverable_error(get_last_error());
	}
}
