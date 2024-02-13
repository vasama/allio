#include <allio/impl/win32/completion_port.hpp>
#include <allio/impl/win32/handles/directory.hpp>
#include <allio/test/filesystem.hpp>
#include <allio/test/win32/completion_port.hpp>

#include <vsm/out_resource.hpp>

#include <catch2/catch_all.hpp>

using namespace allio;
using namespace allio::win32;

//TODO: Test SetFilePointer on directories

TEST_CASE("Asynchronous directory read with IOCP", "[windows][directory]")
{
	detail::unique_handle h;
	{
		//TODO:
		auto const path = std::wstring_view(L"\\??\\C:\\Users\\Untelo\\Downloads");

		UNICODE_STRING unicode_path;
		unicode_path.Buffer = const_cast<wchar_t*>(path.data());
		unicode_path.Length = static_cast<USHORT>(path.size() * sizeof(wchar_t));
		unicode_path.MaximumLength = unicode_path.Length;

		OBJECT_ATTRIBUTES object_attributes = {};
		object_attributes.Length = sizeof(object_attributes);
		object_attributes.ObjectName = &unicode_path;

		LARGE_INTEGER allocation_size;
		allocation_size.QuadPart = 0;

		IO_STATUS_BLOCK io_status_block;

		NTSTATUS const status = win32::NtCreateFile(
			vsm::out_resource(h),
			SYNCHRONIZE | FILE_GENERIC_READ,
			&object_attributes,
			&io_status_block,
			&allocation_size,
			/* FileAttributes: */ 0,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			FILE_OPEN,
			FILE_DIRECTORY_FILE,
			/* EaBuffer: */ nullptr,
			/* EaLength: */ 0);

		REQUIRE(NT_SUCCESS(status));
		REQUIRE(status != STATUS_PENDING);
	}

#if 0
	auto const completion_port = create_completion_port(1).value();

	set_completion_information(
		h.get(),
		completion_port.get(),
		/* key_context: */ nullptr).value();
#endif

	detail::unique_handle event;
	REQUIRE(NT_SUCCESS(win32::NtCreateEvent(
		vsm::out_resource(event),
		SYNCHRONIZE | EVENT_ALL_ACCESS,
		/* ObjectAttributes: */ nullptr,
		SynchronizationEvent,
		/* InitialState: */ FALSE)));


	std::byte storage[4096];
	IO_STATUS_BLOCK io_status_block;

	NTSTATUS const status = NtQueryDirectoryFileEx(
		h.get(),
		event.get(),
		/* ApcRoutine: */ nullptr,
		/* ApcContext: */ nullptr,
		&io_status_block,
		storage,
		sizeof(storage),
		FileIdFullDirectoryInformation,
		/* Flags: */ 0,
		/* FileName: */ nullptr);

	REQUIRE(NT_SUCCESS(status));

	if (status == STATUS_PENDING)
	{
		REQUIRE(WaitForSingleObject(event.get(), INFINITE) == WAIT_OBJECT_0);

#if 0
		test::completion_storage<2> completions;
		REQUIRE(completions.remove(completion_port) == 1);
		completions.get(io_status_block);
#endif

		REQUIRE(NT_SUCCESS(io_status_block.Status));
	}
}
