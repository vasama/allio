#pragma once

#include <allio/platform_handle.hpp>
#include <allio/win32/detail/unique_handle.hpp>
#include <allio/win32/platform.hpp>

#include <vsm/assert.h>
#include <vsm/unique_resource.hpp>

#include <win32.h>

namespace allio {

struct platform_handle::implementation : base_type::implementation
{
	allio_handle_implementation_flags
	(
		overlapped,
		skip_completion_port_on_success,
		skip_handle_event_on_completion,
	);
};

namespace win32 {

struct unique_handle_with_flags
{
	unique_handle handle;
	handle_flags flags;

	unique_handle_with_flags() = default;

	unique_handle_with_flags(HANDLE const handle, handle_flags const flags)
		: handle(handle)
		, flags(flags)
	{
	}
};

template<std::derived_from<platform_handle> Handle, typename Create>
vsm::result<void> initialize_platform_handle(Handle& managed_handle, unique_handle&& handle, Create&& create)
{
	return vsm::consume_resources([&](HANDLE const h) -> vsm::result<void>
	{
		return managed_handle.set_native_handle(static_cast<Create&&>(create)(wrap_handle(h)));
	}, static_cast<unique_handle&&>(handle));
}

template<std::derived_from<platform_handle> Handle>
vsm::result<void> consume_platform_handle(Handle& managed_handle, handle::native_handle_type const native_handle, unique_handle&& handle, auto&&... args)
{
	return vsm::consume_resources([&](HANDLE const handle) -> vsm::result<void>
	{
		return managed_handle.set_native_handle(
		{
			platform_handle::native_handle_type
			{
				native_handle,
				wrap_handle(handle),
			},
			static_cast<decltype(args)&&>(args)...
		});
	}, static_cast<unique_handle&&>(handle));
}

template<typename Handle>
vsm::result<unique_handle_with_flags> consume_or_duplicate_handle(Handle&& managed_handle)
	requires std::derived_from<std::remove_cvref_t<Handle>, platform_handle>
{
	if constexpr (std::is_same_v<Handle, std::remove_cvref_t<Handle>&&>)
	{
		vsm_try(h, managed_handle.release_native_handle());
		return vsm::result<unique_handle_with_flags>(vsm::result_value, unwrap_handle(h.handle), h.flags);
	}
	else
	{
		auto const h = managed_handle.get_native_handle();
		vsm_try(handle, duplicate_handle(unwrap_handle(h.handle)));
		return vsm::result<unique_handle_with_flags>(vsm::result_value, handle.release(), h.flags);
	}
}


handle_flags set_multiplexable_completion_modes(HANDLE handle);

vsm::result<unique_handle> duplicate_handle(HANDLE handle);

} // namespace win32
} // namespace allio
