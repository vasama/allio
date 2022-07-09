#pragma once

#include <allio/detail/new.h>

#include <vsm/platform.h>

namespace allio::detail {

using allocation = allio_allocation;

[[nodiscard]] vsm_always_inline inline allocation acquire_storage(
	size_t const min_size,
	size_t const max_size,
	size_t const alignment,
	bool const automatic)
{
	return allio_acquire_storage(min_size, max_size, alignment, automatic);
}

vsm_always_inline inline void release_storage(
	void* const storage,
	size_t const size_hint,
	size_t const alignment,
	bool const automatic)
{
	return allio_release_storage(storage, size_hint, alignment, automatic);
}

} // namespace allio::detail
