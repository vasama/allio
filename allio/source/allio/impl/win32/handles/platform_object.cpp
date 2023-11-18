#include <allio/impl/win32/handles/platform_object.hpp>

#include <allio/impl/win32/error.hpp>

#include <vsm/out_resource.hpp>

#include <win32.h>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

handle_flags win32::set_file_completion_notification_modes(HANDLE const handle)
{
	handle_flags flags_value = handle_flags::none;

	if (SetFileCompletionNotificationModes(
		handle,
		FILE_SKIP_COMPLETION_PORT_ON_SUCCESS | FILE_SKIP_SET_EVENT_ON_HANDLE))
	{
		using flags = platform_object_t::impl_type::flags;
		flags_value |= flags::skip_completion_port_on_success;
		flags_value |= flags::skip_handle_event_on_completion;
	}

	return flags_value;
}

vsm::result<unique_handle> win32::duplicate_handle(HANDLE const handle)
{
	HANDLE const process = GetCurrentProcess();

	unique_handle duplicate;
	if (!DuplicateHandle(
		/* hSourceProcessHandle: */ process,
		/* hSourceHandle: */ handle,
		/* hTargetProcessHandle: */ process,
		vsm::out_resource(duplicate),
		/* dwDesiredAccess: */ 0,
		/* bInheritHandle: */ false,
		/* dwOptions: */ DUPLICATE_SAME_ACCESS))
	{
		return vsm::unexpected(get_last_error());
	}

	return duplicate;
}


vsm::result<void> platform_object_t::close(
	native_type& h,
	io_parameters_t<close_t> const& args)
{
	if (h.platform_handle != native_platform_handle::null)
	{
		unrecoverable(close_handle(h.platform_handle));
		h.platform_handle = native_platform_handle::null;
	}

	return {};
}
