#include <allio/linux/detail/unique_mmap.hpp>

#include <allio/error.hpp>
#include <allio/impl/linux/error.hpp>

#include <sys/mman.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::linux;

void detail::mmap_deleter::release(void* const addr, size_t const size)
{
	if (munmap(addr, size) == -1)
	{
		unrecoverable_error(get_last_error());
	}
}
