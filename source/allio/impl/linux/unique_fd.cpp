#include <allio/linux/detail/unique_fd.hpp>

#include <unistd.h>

using namespace allio;

void detail::fd_deleter::release(int const fd)
{
	allio_VERIFY(::close(fd) != -1);
}
