#include <allio/detail/mmap.hpp>

#include <allio/error.hpp>
#include <allio/impl/linux/error.hpp>

#include <sys/mman.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

void detail::close_mmap(void* const base, size_t const size)
{
	if (munmap(base, size) == -1)
	{
		unrecoverable_error(get_last_error());
	}
}
