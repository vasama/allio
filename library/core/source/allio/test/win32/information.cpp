#include <allio/detail/unique_handle.hpp>
#include <allio/impl/win32/kernel.hpp>

#include <vsm/out_resource.hpp>

#include <catch2/catch_all.hpp>

using namespace allio;
using namespace allio::win32;

TEST_CASE("Volume sector information can be queried", "[windows]")
{
	detail::unique_handle handle;
	{
		UNICODE_STRING path = make_unicode_string(L"\\??\\C:\\");

		OBJECT_ATTRIBUTES object_attributes =
		{
			.Length = sizeof(object_attributes),
			.ObjectName = &path,
		};

		IO_STATUS_BLOCK io_status_block;
		NTSTATUS const status = win32::NtCreateFile(
			vsm::out_resource(handle),
			SYNCHRONIZE,
			&object_attributes,
			&io_status_block,
			/* AllocationSize: */ nullptr,
			/* FileAttributes: */ 0,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			FILE_OPEN,
			FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
			/* EaBuffer: */ nullptr,
			/* EaLength: */ 0);

		REQUIRE(NT_SUCCESS(status));
	}

	FILE_FS_SECTOR_SIZE_INFORMATION information;

	IO_STATUS_BLOCK io_status_block;
	NTSTATUS const status = win32::NtQueryVolumeInformationFile(
		handle.get(),
		&io_status_block,
		&information,
		sizeof(information),
		win32::FileFsSectorSizeInformation);

	REQUIRE(NT_SUCCESS(status));
}
