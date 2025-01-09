#pragma once

#include <allio/linux/detail/mmap.hpp>
#include <allio/impl/linux/error.hpp>

#include <vsm/lazy.hpp>

#include <sys/mman.h>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

inline vsm::result<detail::unique_mmap<void>> mmap(
	void* const preferred_address,
	size_t const size,
	int const protection,
	int const flags,
	int const fd,
	off_t const offset)
{
	void* const base = ::mmap(
		preferred_address,
		size,
		protection,
		flags,
		fd,
		offset);

	if (base == MAP_FAILED)
	{
		return vsm::unexpected(get_last_error());
	}

	return vsm_lazy(detail::unique_mmap<void>(detail::mmap_view<void>(base, size)));
}

inline vsm::result<void> mprotect(
	void* const base,
	size_t const size,
	int const protection)
{
	if (::mprotect(base, size, protection) == -1)
	{
		return vsm::unexpected(get_last_error());
	}
	return {};
}

inline vsm::result<void> madvise(
	void* const base,
	size_t const size,
	int const advice)
{
	if (::madvise(base, size, advice) == -1)
	{
		return vsm::unexpected(get_last_error());
	}
	return {};
}

} // namespace allio::linux

#include <allio/linux/detail/undef.i>
