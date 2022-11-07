#pragma once

#include <allio/platform_handle.hpp>

#include <allio/linux/detail/unique_fd.hpp>
#include <allio/linux/platform.hpp>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

using detail::unique_fd;

template<std::derived_from<platform_handle> Handle>
result<void> consume_platform_handle(Handle& managed_handle, handle::native_handle_type const handle_handle, unique_fd&& fd, auto&&... args)
{
	return consume_resource(static_cast<unique_fd&&>(fd), [&](int const fd) -> result<void>
	{
		return managed_handle.set_native_handle(
		{
			platform_handle::native_handle_type
			{
				handle_handle,
				wrap_handle(fd),
			},
			static_cast<decltype(args)&&>(args)...
		});
	});
}

} // namespace allio::linux

#include <allio/linux/detail/undef.i>
