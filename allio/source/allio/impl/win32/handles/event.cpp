#include <allio/detail/handles/event.hpp>

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

vsm::result<void> event_t::create(
	native_type& h,
	io_parameters_t<event_t, event_io::create_t> const& a)
{
	vsm_try_void(kernel_init());

	handle_flags flags = flags::none;
	EVENT_TYPE event_type = NotificationEvent;

	if (a.mode == event_mode::auto_reset)
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
		a.signal);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	h = native_type
	{
		platform_object_t::native_type
		{
			{
				flags::not_null | flags,
			},
			wrap_handle(handle.release()),
		}
	};

	return {};
}

vsm::result<void> event_t::signal(
	native_type const& h,
	io_parameters_t<event_t, event_io::signal_t> const&)
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

vsm::result<void> event_t::reset(
	native_type const& h,
	io_parameters_t<event_t, event_io::reset_t> const&)
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

vsm::result<void> event_t::wait(
	native_type const& h,
	io_parameters_t<event_t, event_io::wait_t> const& a)
{
	NTSTATUS const status = win32::NtWaitForSingleObject(
		unwrap_handle(h.platform_handle),
		/* Alertable: */ false,
		kernel_timeout(a.deadline));

	//TODO: Check for STATUS_TIMEOUT in other places where it might be missing.
	//TODO: Replace NT_SUCCESS with one including a debug hook for warning and information statuses.
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
