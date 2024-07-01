#include <allio/impl/new.hpp>

#include <vsm/weak.h>

#include <new>

using namespace allio;
using namespace allio::detail;

extern "C"
allio::detail::allocation vsm_weak(allio_acquire_storage)(size_t const min_size, size_t /* max_size */, size_t const alignment, bool /* automatic */)
{
	void* const storage = operator new(min_size, static_cast<std::align_val_t>(alignment), std::nothrow);
	return { storage, storage != nullptr ? min_size : static_cast<size_t>(0) };
}

extern "C"
void vsm_weak(allio_release_storage)(void* const storage, size_t /* size_hint */, size_t const alignment, bool /* automatic */)
{
	operator delete(storage, static_cast<std::align_val_t>(alignment), std::nothrow);
}


allocation allio::detail::acquire_storage(size_t const min_size, size_t const max_size, size_t const alignment, bool const automatic)
{
	return allio_acquire_storage(min_size, max_size, alignment, automatic);
}

void allio::detail::release_storage(void* const storage, size_t const size_hint, size_t const alignment, bool const automatic)
{
	return allio_release_storage(storage, size_hint, alignment, automatic);
}
