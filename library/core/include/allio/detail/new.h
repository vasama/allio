#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum allio_allocation_strategy
{
	allio_allocation_strategy_generic,
	allio_allocation_strategy_automatic,
	allio_allocation_strategy_buffering,
};

struct allio_allocation
{
	void* storage;
	size_t size;
};

allio_allocation allio_acquire_storage(
	size_t min_size,
	size_t max_size,
	size_t alignment,
	allio_allocation_strategy strategy);

extern "C"
void allio_release_storage(
	void* storage,
	size_t size_hint,
	size_t alignment,
	allio_allocation_strategy strategy);

#ifdef __cplusplus
} // extern "C"
#endif
