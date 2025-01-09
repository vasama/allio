#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct allio_allocation
{
	void* storage;
	size_t size;
};

allio_allocation allio_acquire_storage(
	size_t min_size,
	size_t max_size,
	size_t alignment,
	bool automatic);

extern "C"
void allio_release_storage(
	void* storage,
	size_t size_hint,
	size_t alignment,
	bool automatic);

#ifdef __cplusplus
} // extern "C"
#endif
