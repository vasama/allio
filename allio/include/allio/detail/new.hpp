#pragma once

#include <cstddef>

namespace allio::detail {

struct allocation
{
	void* memory;
	size_t size;
};

allocation memory_acquire(size_t min_size, size_t max_size, size_t alignment, bool automatic);
void memory_release(void* memory, size_t size_hint, size_t alignment, bool automatic);

} // namespace allio::detail

extern "C"
allio::detail::allocation allio_memory_acquire(size_t min_size, size_t max_size, size_t alignment, bool automatic);

extern "C"
void allio_memory_release(void* memory, size_t size_hint, size_t alignment, bool automatic);

