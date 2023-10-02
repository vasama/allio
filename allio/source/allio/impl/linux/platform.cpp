#include <allio/platform.hpp>

#include <allio/linux/error.hpp>
#include <allio/linux/platform.hpp>

#include <unistd.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

vsm::result<void> detail::close_handle(native_platform_handle const handle) noexcept
{
	static_assert(vsm_os_linux, "Check close behaviour on EINTR");

	if (::close(fd) == -1)
	{
		int const e = errno;

		if (e == EBADF)
		{
			return vsm::unexpected(static_cast<system_error>(e));
		}

		unrecoverable_error(static_cast<system_error>(e));
	}

	return {};
}
