#include <allio/impl/win32/event.hpp>

#include <allio/impl/error_encoding.hpp>
#include <allio/impl/win32/kernel.hpp>

#include <vsm/out_resource.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

vsm::result<detail::unique_handle> win32::create_event(
	bool const auto_reset,
	bool const initially_signaled)
{
	vsm::result<unique_handle> r(vsm::result_value);

	NTSTATUS const status = NtCreateEvent(
		vsm::out_resource(*r),
		EVENT_ALL_ACCESS,
		/* ObjectAttributes: */ nullptr,
		auto_reset
			? SynchronizationEvent
			: NotificationEvent,
		initially_signaled);

	if (!NT_SUCCESS(status))
	{
		r = vsm::unexpected(allio_error(static_cast<kernel_error>(status)));
	}

	return r;
}

vsm::result<void> win32::signal_event(HANDLE const event)
{
	NTSTATUS const status = NtSetEvent(
		event,
		/* PreviousState: */ nullptr);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(allio_error(static_cast<kernel_error>(status)));
	}

	return {};
}
