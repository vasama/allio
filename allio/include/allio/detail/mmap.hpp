#pragma once

#include <vsm/standard.hpp>
#include <vsm/unique_resource.hpp>

#include <cstddef>

namespace allio::detail {

void close_mmap(void* base, size_t size);

template<typename T>
struct mmap_view
{
	T* base;
	size_t size;
};

struct mmap_deleter
{
	template<typename T>
	void vsm_static_operator_invoke(mmap_view<T> const view)
	{
		close_mmap(view.base, view.size);
	}
};

template<typename T>
using unique_mmap = vsm::unique_resource<mmap_view<T>, mmap_deleter>;

} // namespace allio::detail
