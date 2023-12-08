#include <allio/impl/new.hpp>

#include <vsm/weak.h>

#include <new>

using namespace allio;
using namespace allio::detail;

extern "C"
allio::detail::allocation vsm_weak(allio_memory_acquire)(size_t const min_size, size_t /* max_size */, size_t const alignment, bool /* automatic */)
{
	void* const memory = operator new(min_size, static_cast<std::align_val_t>(alignment), std::nothrow);
	return { memory, memory != nullptr ? min_size : static_cast<size_t>(0) };
}

extern "C"
void vsm_weak(allio_memory_release)(void* const memory, size_t /* size_hint */, size_t const alignment, bool /* automatic */)
{
	operator delete(memory, static_cast<std::align_val_t>(alignment), std::nothrow)
}


allocation allio::detail::memory_acquire(size_t const min_size, size_t const max_size, size_t const alignment, bool const automatic)
{
	return allio_memory_acquire(min_size, max_size, alignment, automatic);
}

void allio::detail::memory_release(void* const memory, size_t const size_hint, size_t const alignment, bool const automatic)
{
	return allio_memory_release(memory, size_hint, alignment, automatic);
}
