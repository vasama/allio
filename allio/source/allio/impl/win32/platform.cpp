#include <allio/platform.hpp>

#include <allio/win32/error.hpp>
#include <allio/win32/kernel_error.hpp>
#include <allio/win32/platform.hpp>
#include <allio/impl/win32/kernel.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

vsm::result<void> detail::close_handle(native_platform_handle const handle, error_handler* const error_handler) noexcept
{
	NTSTATUS const status = NtClose(unwrap_handle(handle));

	if (!NT_SUCCESS(status))
	{
		switch (status)
		{
		case STATUS_INVALID_HANDLE:
		case STATUS_HANDLE_NOT_CLOSABLE:
			return vsm::unexpected(static_cast<kernel_error>(status));
		}

		get_error_handler(error_handler).handle_error(
		{
			.error = static_cast<kernel_error>(status),
			.reason = error_source::NtClose,
		});
	}
	
	return {};
}

vsm::result<void> detail::close_handle(native_platform_handle const handle) noexcept
{
	return close_handle(handle, nullptr);
}
