#pragma once

#include <allio/detail/assert.hpp>
#include <allio/platform_handle.hpp>

#include <bit>
#include <type_traits>

typedef void* HANDLE;

namespace allio::win32 {

template<typename Handle = HANDLE>
inline native_platform_handle wrap_handle(std::type_identity_t<Handle> const handle)
{
	allio_ASSERT(std::bit_cast<uintptr_t>(handle) != static_cast<uintptr_t>(-1));
	return std::bit_cast<native_platform_handle>(handle);
}

template<typename Handle = HANDLE>
inline Handle unwrap_handle(native_platform_handle const handle)
{
	return std::bit_cast<Handle>(handle);
}

} // namespace allio::win32
