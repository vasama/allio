#include <allio/linux/detail/unique_fd.hpp>

#include <allio/impl/linux/error.hpp>

#include <unistd.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::linux;

vsm::result<void> linux::close_fd(int const fd) noexcept
{
	if (::close(fd) == -1)
	{
		return vsm::unexpected(get_last_error());
	}
	return {};
}
