#include <allio/impl/new.hpp>

#include <new>

using namespace allio;
using namespace allio::detail;

extern "C"
allio_allocation allio_acquire_storage(
	size_t const min_size,
	size_t /* max_size */,
	size_t const alignment,
	bool /* automatic */)
{
	void* const storage = operator new(
		min_size,
		static_cast<std::align_val_t>(alignment),
		std::nothrow);

	return
	{
		storage,
		storage != nullptr
			? min_size
			: static_cast<size_t>(0),
	};
}

extern "C"
void allio_release_storage(
	void* const storage,
	size_t /* size_hint */,
	size_t const alignment,
	bool /* automatic */)
{
	operator delete(storage, static_cast<std::align_val_t>(alignment), std::nothrow);
}
