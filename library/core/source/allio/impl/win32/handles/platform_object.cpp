#include <allio/impl/win32/handles/platform_object.hpp>

#include <allio/detail/serialization.hpp>
#include <allio/impl/error_encoding.hpp>
#include <allio/impl/win32/error.hpp>
#include <allio/impl/win32/kernel.hpp>

#include <vsm/out_resource.hpp>

#include <Windows.h>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

bool win32::verify_object_type(HANDLE const handle, std::wstring_view const type)
{
	struct
	{
		struct
		{
			OBJECT_TYPE_INFORMATION information;
			WCHAR typename_storage[32];
		}
		storage;

		// In some versions of Windows for certain object types, ObjectTypeInformation may overrun
		// the provided buffer by at least one WCHAR (null terminator). For this reason, a generous
		// amount of padding is added after the actual information buffer.
		UCHAR padding[8];
	}
	storage;

	ULONG returned_size;
	NTSTATUS const status = win32::NtQueryObject(
		handle,
		ObjectTypeInformation,
		&storage,
		sizeof(storage.storage),
		&returned_size);

	if (!NT_SUCCESS(status))
	{
		return false;
	}

	return get_unicode_string(storage.storage.information.TypeName) == type;
}

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
		return vsm::unexpected(allio_error(static_cast<kernel_error>(status)));
	}

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
		return vsm::unexpected(allio_error(get_last_error()));
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
	native_handle<platform_object_t>& h,
	io_parameters_t<object_t, close_t> const&)
{
	if (h.platform_handle != native_platform_handle::null &&
		!h.flags[impl_type::flags::pseudo_handle])
	{
		close_platform_handle(unwrap_handle(h.platform_handle));
	}
	h = {};
	return {};
}

vsm::result<void> platform_object_t::serializer_visit(
	native_handle<platform_object_t>& h,
	serialization_context& serializer)
{
	vsm_try_void(base_type::serializer_visit(h, serializer));
	vsm_try_void(serializer.visit(h.platform_handle));

	// The size of Windows HANDLE depends on the target architecture; either 32-bit or 64-bit. In
	// order to maintain compatibility between 32-bit and 64-bit processes, when targeting a 32-bit
	// architecture an additional 32 bits containing the high bit of the handle value are written
	// after the handle. Being that Windows is always little-endian, this effectively sign-extends
	// the handle value to 64-bits.
	if constexpr (sizeof(h.platform_handle) == sizeof(uint32_t))
	{
		int32_t extension = static_cast<int32_t>(h.platform_handle) >= 0
			? 0
			: -1;

		vsm_try_void(serializer.visit(extension));
	}

	return {};
}
