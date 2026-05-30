#include <allio/impl/win32/completion_port.hpp>
#include <allio/impl/win32/event.hpp>
#include <allio/impl/win32/handles/directory.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/test/win32/completion_port.hpp>

#include <vsm/out_resource.hpp>

#include <catch2/catch_all.hpp>

#include <filesystem>

using namespace allio;
using namespace allio::win32;

static HANDLE get_current_directory()
{
	// Normally this would be risky due to potential concurrent mutations, but these tests do not
	// change the current directory, so in this case it is not problematic.
	HANDLE const handle = NtCurrentPeb()->ProcessParameters->CurrentDirectoryHandle;

	REQUIRE(handle != NULL);

	return handle;
}

TEST_CASE("Directory seek using SetFilePointer", "[windows][directory]")
{
	HANDLE const h = get_current_directory();

	FILE_POSITION_INFORMATION information =
	{
		.CurrentByteOffset = make_large_integer(0),
	};

	IO_STATUS_BLOCK io_status_block;

	NTSTATUS const status = win32::NtSetInformationFile(
		h,
		&io_status_block,
		&information,
		sizeof(information),
		FilePositionInformation);

	REQUIRE(NT_SUCCESS(status));
}

#if 0
TEST_CASE("Asynchronous directory read with IOCP", "[windows][directory]")
{
	HANDLE const h = get_current_directory();

#if 0 //TODO: Determine if IOCP completions are possible at all.
	auto const completion_port = create_completion_port(1).value();

	set_completion_information(
		h,
		completion_port.get(),
		/* key_context: */ nullptr).value();
#endif

	auto const event = create_event().value();

	std::byte storage[4096];
	IO_STATUS_BLOCK io_status_block;

	NTSTATUS const status = win32::NtQueryDirectoryFileEx(
		h,
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
		REQUIRE(WaitForSingleObject(
			event.get(),
			/* dwMilliseconds: */ 10'000) == WAIT_OBJECT_0);

#if 0
		test::completion_storage<2> completions;
		REQUIRE(completions.remove(completion_port) == 1);
		completions.get(io_status_block);
#endif

		REQUIRE(NT_SUCCESS(io_status_block.Status));
	}
}
#endif
