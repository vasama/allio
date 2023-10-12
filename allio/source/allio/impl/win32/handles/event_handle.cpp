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

vsm::result<void> event_handle_t::signal() const
{
	if (!*this)
	{
		return vsm::unexpected(error::handle_is_null);
	}

	NTSTATUS const status = NtSetEvent(
		unwrap_handle(get_platform_handle()),
		/* PreviousState: */ nullptr);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return {};
}

vsm::result<void> event_handle_t::reset() const
{
	if (!*this)
	{
		return vsm::unexpected(error::handle_is_null);
	}

	NTSTATUS const status = NtResetEvent(
		unwrap_handle(get_platform_handle()),
		/* PreviousState: */ nullptr);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return {};
}

vsm::result<void> event_handle_t::do_blocking_io(
	event_handle_t& h,
	io_result_ref_t<create_t>,
	io_parameters_t<create_t> const& args)
{
	vsm_try_void(kernel_init());

	if (h)
	{
		return vsm::unexpected(error::handle_is_not_null);
	}

	handle_flags flags = flags::none;
	EVENT_TYPE event_type = NotificationEvent;

	if (args.reset_mode == reset_mode::auto_reset)
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

	platform_handle::native_handle_type const native =
	{
		{
			flags::not_null | flags,
		},
		wrap_handle(handle.get()),
	};

	vsm_assert(check_native_handle(native));
	h.set_native_handle(native);

	(void)handle.release();

	return {};
}

vsm::result<void> event_handle_t::do_blocking_io(
	event_handle_t const& h,
	io_result_ref_t<wait_t>,
	io_parameters_t<wait_t> const& args)
{
	if (!h)
	{
		return vsm::unexpected(error::handle_is_null);
	}

	NTSTATUS const status = win32::NtWaitForSingleObject(
		unwrap_handle(h.get_platform_handle()),
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
