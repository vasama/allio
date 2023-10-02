#include <allio/platform.hpp>

#include <allio/win32/error.hpp>
#include <allio/win32/kernel_error.hpp>
#include <allio/win32/platform.hpp>
#include <allio/impl/win32/kernel.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

vsm::result<void> detail::close_handle(native_platform_handle const handle) noexcept
{
	NTSTATUS const status = win32::NtClose(unwrap_handle(handle));

	if (!NT_SUCCESS(status))
	{
		switch (status)
		{
		case STATUS_INVALID_HANDLE:
		case STATUS_HANDLE_NOT_CLOSABLE:
			return vsm::unexpected(static_cast<kernel_error>(status));
		}
		
		unrecoverable_error(static_cast<kernel_error>(status));
	}
	
	return {};
}
