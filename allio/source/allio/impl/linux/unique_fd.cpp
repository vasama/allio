#include <allio/linux/detail/unique_fd.hpp>

#include <allio/impl/linux/error.hpp>

#include <unistd.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::linux;

vsm::result<void> detail::close_fd(int const fd) noexcept
{
	static_assert(vsm_os_linux, "Check close behaviour on EINTR");

	if (::close(fd) == -1)
	{
		int const e = errno;

		if (e == EBADF)
		{
			return vsm::unexpected(static_cast<system_error>(e));
		}

		
	}
	return {};
}
