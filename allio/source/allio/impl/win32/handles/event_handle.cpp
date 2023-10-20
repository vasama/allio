#include <allio/detail/handles/event_handle.hpp>

#include <allio/impl/win32/error.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/win32/detail/unique_handle.hpp>
#include <allio/win32/kernel_error.hpp>
#include <allio/win32/platform.hpp>

#include <vsm/out_resource.hpp>

#include <win32.h>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

vsm::result<void> event_handle_t::blocking_io(native_type& h, io_parameters_t<create_t> const& args)
{
	vsm_try_void(kernel_init());

	handle_flags flags = flags::none;
	EVENT_TYPE event_type = NotificationEvent;

	if (args.reset_mode == event_reset_mode::auto_reset)
	{
		flags |= flags::auto_reset;
		event_type = SynchronizationEvent;
	}

	unique_handle handle;
	NTSTATUS const status = NtCreateEvent(
		vsm::out_resource(handle),
		EVENT_ALL_ACCESS,
		/* ObjectAttributes: */ nullptr,
		event_type,
		args.signal);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	h = platform_handle_t::native_type
	{
		{
			flags::not_null | flags,
		},
		wrap_handle(handle.release()),
	};

	return {};
}

vsm::result<void> event_handle_t::blocking_io(native_type const& h, io_parameters_t<signal_t> const& args)
{
	NTSTATUS const status = NtSetEvent(
		unwrap_handle(h.platform_handle),
		/* PreviousState: */ nullptr);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return {};
}

vsm::result<void> event_handle_t::blocking_io(native_type const& h, io_parameters_t<reset_t> const& args)
{
	NTSTATUS const status = NtResetEvent(
		unwrap_handle(h.platform_handle),
		/* PreviousState: */ nullptr);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return {};
}

vsm::result<void> event_handle_t::blocking_io(native_type const& h, io_parameters_t<wait_t> const& args)
{
	NTSTATUS const status = win32::NtWaitForSingleObject(
		unwrap_handle(h.platform_handle),
		/* Alertable: */ false,
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
