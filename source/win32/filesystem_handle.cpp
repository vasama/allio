#include "filesystem_handle.hpp"

#include <allio/win32/kernel_error.hpp>

#include "api_string.hpp"
#include "kernel.hpp"

using namespace allio;
using namespace allio::win32;

result<unique_handle> win32::create_file(filesystem_handle const* const base, path_view const path, file_parameters const& args)
{
	allio_TRYV(kernel_init());
	
	DWORD desired_access = SYNCHRONIZE;
	DWORD attributes = 0;
	DWORD share_access = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
	DWORD create_disposition = FILE_OPEN;
	DWORD create_options = FILE_NON_DIRECTORY_FILE;

	if ((args.handle_flags & flags::multiplexable) == flags::none)
	{
		create_options |= FILE_SYNCHRONOUS_IO_NONALERT;
	}

	switch (args.mode)
	{
	case file_mode::none:
		break;

	case file_mode::attr_read:
		desired_access |= FILE_WRITE_ATTRIBUTES | STANDARD_RIGHTS_WRITE | DELETE;
		break;

	case file_mode::attr_write:
		desired_access |= FILE_READ_ATTRIBUTES | STANDARD_RIGHTS_READ;
		break;

	case file_mode::read:
		desired_access |= GENERIC_READ;
		break;

	case file_mode::write:
		desired_access |= GENERIC_WRITE | DELETE;
		break;

	case file_mode::append:
		desired_access |= FILE_APPEND_DATA | GENERIC_READ | FILE_WRITE_ATTRIBUTES | STANDARD_RIGHTS_WRITE;
		break;
	}

	switch (args.creation)
	{
	case file_creation::open_existing:
		create_disposition = FILE_OPEN;
		break;

	case file_creation::create_only:
		create_disposition = FILE_CREATE;
		break;

	case file_creation::open_or_create:
		create_disposition = FILE_OPEN_IF;
		break;

	case file_creation::truncate_existing:
		create_disposition = FILE_OVERWRITE;
		break;

	case file_creation::replace_existing:
		create_disposition = FILE_SUPERSEDE;
		break;
	}

	api_string nt_path;
	nt_path.append(L"\\??\\");
	nt_path.append(path);

	UNICODE_STRING unicode_string;
	unicode_string.Buffer = nt_path.data();
	unicode_string.Length = nt_path.size() * sizeof(wchar_t);
	unicode_string.MaximumLength = unicode_string.Length + sizeof(wchar_t);

	OBJECT_ATTRIBUTES oa = {};
	oa.Length = sizeof(oa);
	oa.RootDirectory = base != nullptr
		? unwrap_handle(base->get_platform_handle())
		: NULL;
	oa.ObjectName = &unicode_string;

	LARGE_INTEGER allocation_size;
	allocation_size.QuadPart = 0;

	IO_STATUS_BLOCK isb = make_io_status_block();

	HANDLE handle = INVALID_HANDLE_VALUE;
	NTSTATUS status = win32::NtCreateFile(
		&handle,
		desired_access,
		&oa,
		&isb,
		&allocation_size,
		attributes,
		share_access,
		create_disposition,
		create_options,
		nullptr,
		0);

	if (status == STATUS_PENDING)
	{
		status = io_wait(handle, &isb, deadline());
	}

	if (status < 0)
	{
		return allio_ERROR(static_cast<kernel_error>(status));
	}

	return { result_value, handle };
}
