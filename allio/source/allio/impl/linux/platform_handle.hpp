#pragma once

#include <allio/platform_handle.hpp>
#include <allio/linux/detail/unique_fd.hpp>
#include <allio/linux/platform.hpp>

#include <vsm/utility.hpp>

#include <allio/linux/detail/undef.i>

namespace allio {

struct platform_handle::implementation : base_type::implementation {};

namespace linux {

template<std::derived_from<platform_handle> Handle>
vsm::result<void> initialize_platform_handle(Handle& managed_handle, unique_fd&& fd, auto&& create)
{
	return vsm::consume_resources([&](int const h) -> vsm::result<void>
	{
		return managed_handle.set_native_handle(vsm_forward(create)(wrap_handle(h)));
	}, vsm_move(fd));
}

#if 0
template<std::derived_from<platform_handle> Handle>
vsm::result<void> consume_platform_handle(Handle& managed_handle, handle::native_handle_type const handle_handle, unique_fd&& fd, auto&&... args)
{
	return vsm::consume_resources([&](int const fd) -> vsm::result<void>
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
	}, static_cast<unique_fd&&>(fd));
}
#endif

} // namespace linux
} // namespace allio

#include <allio/linux/detail/undef.i>
