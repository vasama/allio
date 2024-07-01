#pragma once

#include <cstddef>

namespace allio::detail {

struct allocation
{
	void* storage;
	size_t size;
};

allocation acquire_storage(size_t min_size, size_t max_size, size_t alignment, bool automatic);
void release_storage(void* storage, size_t size_hint, size_t alignment, bool automatic);

} // namespace allio::detail

extern "C"
allio::detail::allocation allio_acquire_storage(size_t min_size, size_t max_size, size_t alignment, bool automatic);

extern "C"
void allio_release_storage(void* storage, size_t size_hint, size_t alignment, bool automatic);
