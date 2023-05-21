#include <allio/linux/detail/unique_fd.hpp>

#include <unistd.h>

using namespace allio;

void detail::fd_deleter::release(int const fd)
{
	vsm_verify(::close(fd) != -1);
}
