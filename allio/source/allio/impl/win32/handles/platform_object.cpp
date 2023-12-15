#include <allio/impl/win32/handles/platform_object.hpp>

#include <allio/impl/win32/error.hpp>
#include <allio/impl/win32/kernel.hpp>

#include <vsm/out_resource.hpp>

#include <win32.h>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

vsm::result<ACCESS_MASK> win32::get_handle_access(HANDLE const handle)
{
	OBJECT_BASIC_INFORMATION information;

	ULONG returned_size;
	NTSTATUS const status = win32::NtQueryObject(
		handle,
		ObjectBasicInformation,
		&information,
		sizeof(information),
		&returned_size);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	static constexpr size_t field_extent =
		offsetof(OBJECT_BASIC_INFORMATION, GrantedAccess) +
		sizeof(OBJECT_BASIC_INFORMATION::GrantedAccess);

	vsm_assert(returned_size >= field_extent);

	return information.GrantedAccess;
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


vsm::result<void> platform_object_t::close(
	native_type& h,
	io_parameters_t<object_t, close_t> const&)
{
	if (!h.flags[impl_type::flags::pseudo_handle])
	{
		close_platform_handle(unwrap_handle(h.platform_handle));
	}
	h = {};
	return {};
}
