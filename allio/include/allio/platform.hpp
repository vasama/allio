#pragma once

#include <allio/error.hpp>

#include <vsm/result.hpp>
#include <vsm/standard.hpp>
#include <vsm/unique_resource.hpp>

#include <cstdint>

namespace allio {

enum class native_platform_handle : uintptr_t
{
	null = 0
};

namespace detail {

vsm::result<void> close_native_platform_handle(native_platform_handle handle) noexcept;

struct native_platform_handle_deleter
{
	void vsm_static_operator_invoke(native_platform_handle const handle) noexcept
	{
		unrecoverable(close_native_platform_handle(handle));
	}
};

} // namespace detail

using unique_native_platform_handle = vsm::unique_resource<
	native_platform_handle,
	detail::native_platform_handle_deleter,
	native_platform_handle::null>;

} // namespace allio
