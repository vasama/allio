#include <allio/event_handle.hpp>

#include <allio/impl/win32/error.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/impl/win32/platform_handle.hpp>
#include <allio/win32/nt_error.hpp>

#include <win32.h>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

vsm::result<void> event_handle_base::reset() const
{
	if (!*this)
	{
		return vsm::unexpected(error::handle_is_null);
	}

	if (!ResetEvent(unwrap_handle(get_platform_handle())))
	{
		return vsm::unexpected(get_last_error());
	}

	return {};
}

vsm::result<void> event_handle_base::signal() const
{
	if (!*this)
	{
		return vsm::unexpected(error::handle_is_null);
	}

	if (!SetEvent(unwrap_handle(get_platform_handle())))
	{
		return vsm::unexpected(get_last_error());
	}

	return {};
}


vsm::result<void> event_handle_base::sync_impl(io::parameters_with_result<io::event_create> const& args)
{
	vsm_try_void(kernel_init());

	event_handle& h = *args.handle;

	if (h)
	{
		return vsm::unexpected(error::handle_is_not_null);
	}

	handle_flags h_flags = {};
	bool manual_reset = true;

	if (args.auto_reset)
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

vsm::result<void> event_handle_base::sync_impl(io::parameters_with_result<io::event_wait> const& args)
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

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<nt_error>(status));
	}

	return {};
}

allio_handle_implementation(event_handle);
