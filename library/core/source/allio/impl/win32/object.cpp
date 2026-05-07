#include <allio/impl/win32/object.hpp>

#include <allio/impl/error_encoding.hpp>
#include <allio/impl/syscall.hpp>
#include <allio/impl/win32/error.hpp>

#include <vsm/out_resource.hpp>

using namespace allio;
using namespace allio::win32;
using namespace allio::detail;

vsm::result<ACCESS_MASK> win32::get_handle_access(HANDLE const handle)
{
	OBJECT_BASIC_INFORMATION information;

	ULONG returned_size;
	NTSTATUS const status = allio_syscall(win32::NtQueryObject)(
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
