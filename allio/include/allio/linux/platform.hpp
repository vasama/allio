#pragma once

#include <allio/detail/platform.h>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

inline native_platform_handle wrap_handle(int const handle)
{
	return static_cast<native_platform_handle>(static_cast<uintptr_t>(handle) + 1);
}

inline int unwrap_handle(native_platform_handle const handle)
{
	return static_cast<int>(static_cast<uintptr_t>(handle) - 1);
}

template<typename T>
inline native_platform_handle wrap_handle(T* const handle)
{
	return static_cast<native_platform_handle>(reinterpret_cast<uintptr_t>(handle));
}

template<typename T>
inline T* unwrap_handle(native_platform_handle const handle)
{
	return reinterpret_cast<T*>(static_cast<uintptr_t>(handle));
}

} // namespace allio::linux

#include <allio/linux/detail/undef.i>
