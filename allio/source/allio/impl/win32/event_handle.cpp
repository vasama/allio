#include <allio/detail/handles/event_handle.hpp>

#include <allio/impl/win32/error.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/win32/kernel_error.hpp>
#include <allio/win32/platform.hpp>

#include <win32.h>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

vsm::result<void> _event_handle::_create(reset_mode const reset_mode, create_parameters const& args)
{
	vsm_try_void(kernel_init());

	vsm_assert(!*this);

	handle_flags h_flags = {};
	bool manual_reset = true;

	if (reset_mode == reset_mode::auto_reset)
	{
		manual_reset = false;
		h_flags |= flags::auto_reset;
	}
	
	HANDLE h_handle;
	NTSTATUS const status = NtCreateEvent(
		&h_handle,
		EVENT_ALL_ACCESS,
		/* ObjectAttributes: */ nullptr,
		manual_reset
			? NotificationEvent
			: SynchronizationEvent,
		args.signal);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return {};
}

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

vsm::result<void> _event_handle::_wait(wait_parameters const& args) const
{
	if (!*this)
	{
		return vsm::unexpected(error::handle_is_not_null);
	}

	NTSTATUS const status = win32::NtWaitForSingleObject(
		unwrap_handle(get_platform_handle()),
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
