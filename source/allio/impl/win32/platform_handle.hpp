#pragma once

#include <allio/detail/assert.hpp>
#include <allio/detail/unique_resource.hpp>
#include <allio/platform_handle.hpp>
#include <allio/win32/detail/unique_handle.hpp>
#include <allio/win32/platform.hpp>

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
result<void> initialize_platform_handle(Handle& managed_handle, unique_handle&& handle, Create&& create)
{
	return consume_resource(static_cast<unique_handle&&>(handle), [&](HANDLE const h) -> result<void>
	{
		return managed_handle.set_native_handle(static_cast<Create&&>(create)(wrap_handle(h)));
	});
}

template<std::derived_from<platform_handle> Handle>
result<void> consume_platform_handle(Handle& managed_handle, handle::native_handle_type const native_handle, unique_handle&& handle, auto&&... args)
{
	return consume_resource(static_cast<unique_handle&&>(handle), [&](HANDLE const handle) -> result<void>
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
	});
}

template<typename Handle>
result<unique_handle_with_flags> consume_or_duplicate_handle(Handle&& managed_handle)
	requires std::derived_from<std::remove_cvref_t<Handle>, platform_handle>
{
	if constexpr (std::is_same_v<Handle, std::remove_cvref_t<Handle>&&>)
	{
		allio_TRY(h, managed_handle.release_native_handle());
		return result<unique_handle_with_flags>(result_value, unwrap_handle(h.handle), h.flags);
	}
	else
	{
		auto const h = managed_handle.get_native_handle();
		allio_TRY(handle, duplicate_handle(unwrap_handle(h.handle)));
		return result<unique_handle_with_flags>(result_value, handle.release(), h.flags);
	}
}


handle_flags set_multiplexable_completion_modes(HANDLE handle);

result<unique_handle> duplicate_handle(HANDLE handle);

} // namespace win32
} // namespace allio
