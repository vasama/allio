#pragma once

#include <allio/detail/assert.hpp>
#include <allio/detail/unique_resource.hpp>
#include <allio/platform_handle.hpp>
#include <allio/win32/platform.hpp>

#include <win32.h>

namespace allio::win32 {

result<void> close_handle(HANDLE const handle);

struct handle_deleter
{
	void operator()(HANDLE const handle) const
	{
		allio_VERIFY(close_handle(handle));
	}
};
using unique_handle = detail::unique_resource<HANDLE, handle_deleter, NULL>;

template<std::derived_from<platform_handle> Handle>
result<void> consume_platform_handle(Handle& managed_handle, handle::native_handle_type const handle_handle, unique_handle&& handle, auto&&... args)
{
	return consume_resource(static_cast<unique_handle&&>(handle), [&](HANDLE const handle) -> result<void>
	{
		return managed_handle.set_native_handle(
		{
			platform_handle::native_handle_type
			{
				handle_handle,
				wrap_handle(handle),
			},
			static_cast<decltype(args)&&>(args)...
		});
	});
}

} // namespace allio::win32
