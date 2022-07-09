#include <allio/detail/platform.hpp>

#include <allio/error.hpp>
#include <allio/win32/error.hpp>
#include <allio/win32/kernel_error.hpp>
#include <allio/impl/win32/kernel.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

void detail::close_platform_handle(HANDLE const handle) noexcept
{
	NTSTATUS const status = win32::NtClose(handle);

	if (!NT_SUCCESS(status))
	{
		unrecoverable_error(static_cast<kernel_error>(status));
	}
}
