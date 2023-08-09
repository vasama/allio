#pragma once

#include <allio/platform_handle.hpp>
#include <allio/win32/detail/win32_fwd.hpp>

#include <bit>
#include <type_traits>

namespace allio::win32 {

template<typename Handle = detail::HANDLE>
inline native_platform_handle wrap_handle(std::type_identity_t<Handle> const handle)
{
	return std::bit_cast<native_platform_handle>(handle);
}

template<typename Handle = detail::HANDLE>
inline Handle unwrap_handle(native_platform_handle const handle)
{
	return std::bit_cast<Handle>(handle);
}

} // namespace allio::win32
