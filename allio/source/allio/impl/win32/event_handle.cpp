#include <allio/event_handle.hpp>

#include <allio/impl/win32/error.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/impl/win32/platform_handle.hpp>
#include <allio/win32/kernel_error.hpp>

#include <win32.h>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

vsm::result<void> _event_handle::reset() const
{
	if (!*this)
	{
		return vsm::unexpected(error::handle_is_null);
	}

	NTSTATUS const status = NtResetEvent(
		unwrap_handle(get_platform_handle()),
		nullptr);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return {};
}

vsm::result<void> _event_handle::signal() const
{
	if (!*this)
	{
		return vsm::unexpected(error::handle_is_null);
	}

	NTSTATUS const status = NtSetEvent(
		unwrap_handle(get_platform_handle()),
		nullptr);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return {};
}


vsm::result<void> _event_handle::sync_impl(io::parameters_with_result<io::event_create> const& args)
{
	vsm_try_void(kernel_init());


	event_handle& h = *args.handle;

	if (h)
	{
		return vsm::unexpected(error::handle_is_not_null);
	}

	handle_flags h_flags = {};
	bool manual_reset = true;

	if (args.reset_mode == event_handle::auto_reset)
	{
		manual_reset = false;
		h_flags |= event_handle::flags::auto_reset;
	}

	HANDLE const event = CreateEventW(
		nullptr,
		manual_reset,
		args.signal,
		nullptr);

	if (event == NULL)
	{
		return vsm::unexpected(get_last_error());
	}

	return initialize_platform_handle(h, unique_handle(event),
		[&](native_platform_handle const handle)
		{
			return platform_handle::native_handle_type
			{
				handle::native_handle_type
				{
					h_flags | handle::flags::not_null,
				},
				handle,
			};
		}
	);
}

vsm::result<void> _event_handle::sync_impl(io::parameters_with_result<io::event_wait> const& args)
{
	event_handle const& h = *args.handle;

	if (!h)
	{
		return vsm::unexpected(error::handle_is_not_null);
	}

	NTSTATUS const status = win32::NtWaitForSingleObject(
		unwrap_handle(h.get_platform_handle()),
		false,
		kernel_timeout(args.deadline));

	//TODO: Check for STATUS_TIMEOUT in other places where it might be missing.
	//TODO: Replace NT_SUCCESS with one that checks for non-zero for debug purposes.
	if (status == STATUS_TIMEOUT)
	{
		return vsm::unexpected(error::async_operation_timed_out);
	}

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return {};
}

allio_handle_implementation(event_handle);
