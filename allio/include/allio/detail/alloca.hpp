#pragma once

#include <allio/detail/platform.h>
#include <allio/storage.hpp>

#include <type_traits>

namespace allio::detail {

inline size_t alloca_size(storage_requirements const& requirements)
{
	return requirements.size + requirements.alignment;
}

inline storage_ptr alloca_data(storage_requirements const& requirements, void* const buffer)
{
	size_t const offset = reinterpret_cast<uintptr_t>(buffer) & requirements.alignment - 1;
	return storage_ptr(reinterpret_cast<std::byte*>(buffer) + offset, requirements.size);
}

template<typename Callback>
std::invoke_result_t<Callback&&, storage_ptr> with_alloca(storage_requirements const& requirements, Callback&& callback)
{
	return static_cast<Callback&&>(callback)(
		alloca_data(requirements, vsm_alloca(alloca_size(requirements))));
}

} // namespace allio::detail
