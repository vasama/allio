#include <allio/platform.hpp>

#include <allio/linux/error.hpp>
#include <allio/linux/platform.hpp>

#include <unistd.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

vsm::result<void> detail::close_handle(native_platform_handle const handle, error_handler* const error_handler) noexcept
{
	static_assert(vsm_os_linux, "Check close behaviour on EINTR");

	if (::close(fd) == -1)
	{
		int const e = errno;

		if (e == EBADF)
		{
			return vsm::unexpected(static_cast<system_error>(e));
		}


		get_error_handler(error_handler).handle_error(
		{
			.error = static_cast<kernel_error>(status),
			.reason = error_source::close,
		});
	}

	return {};	
}

vsm::result<void> detail::close_handle(native_platform_handle const handle) noexcept
{
	return close_handle(handle, nullptr);
}
