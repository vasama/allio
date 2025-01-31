#include <allio/detail/unique_handle.hpp>
#include <allio/impl/win32/completion_port.hpp>
#include <allio/test/spawn.hpp>
#include <allio/test/win32/completion_port.hpp>

#include <catch2/catch_all.hpp>

using namespace allio;

namespace {

static detail::unique_handle open_named_pipe_dir()
{
	UNICODE_STRING path = win32::make_unicode_string(L"\\Device\\NamedPipe\\");

	OBJECT_ATTRIBUTES object_attributes = {};
	object_attributes.Length = sizeof(object_attributes);
	object_attributes.ObjectName = &path;

	IO_STATUS_BLOCK io_status_block;

	HANDLE handle;
	NTSTATUS const status = win32::NtCreateFile(
		&handle,
		GENERIC_READ | SYNCHRONIZE,
		&object_attributes,
		&io_status_block,
		/* AllocationSize: */ nullptr,
		/* FileAttributes: */ 0,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		FILE_OPEN,
		FILE_SYNCHRONOUS_IO_NONALERT,
		/* EaBuffer: */ nullptr,
		/* EaLength: */ 0);

	REQUIRE(NT_SUCCESS(status));

	return detail::unique_handle(handle);
}

static detail::unique_handle create_named_pipe()
{
	auto const pipe_dir = open_named_pipe_dir();

	UNICODE_STRING name = {};

	OBJECT_ATTRIBUTES object_attributes = {};
	object_attributes.Length = sizeof(object_attributes);
	object_attributes.RootDirectory = pipe_dir.get();
	object_attributes.ObjectName = &name;
	object_attributes.Attributes = OBJ_CASE_INSENSITIVE;

	IO_STATUS_BLOCK io_status_block;

	HANDLE handle;
	NTSTATUS const status = win32::NtCreateNamedPipeFile(
		&handle,
		GENERIC_READ | SYNCHRONIZE | FILE_READ_ATTRIBUTES,
		&object_attributes,
		&io_status_block,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		FILE_CREATE,
		0,
		/* NamedPipeType: */ 0,
		/* ReadMode: */ 0,
		/* CompletionMode: */ 0,
		/* MaximumInstances: */ 1,
		/* InboundQuota: */ 4096,
		/* OutboundQuota: */ 4096,
		win32::kernel_timeout(std::chrono::milliseconds(INFINITE)));

	REQUIRE(NT_SUCCESS(status));

	return detail::unique_handle(handle);
}

static detail::unique_handle connect_to_named_pipe(HANDLE const server_pipe)
{
	UNICODE_STRING name = {};

	OBJECT_ATTRIBUTES object_attributes = {};
	object_attributes.Length = sizeof(object_attributes);
	object_attributes.RootDirectory = server_pipe;
	object_attributes.ObjectName = &name;
	object_attributes.Attributes = OBJ_CASE_INSENSITIVE;

	IO_STATUS_BLOCK io_status_block;

	HANDLE handle;
	NTSTATUS const status = win32::NtCreateFile(
		&handle,
		GENERIC_WRITE | SYNCHRONIZE | FILE_READ_ATTRIBUTES,
		&object_attributes,
		&io_status_block,
		/* AllocationSize: */ nullptr,
		/* FileAttributes: */ 0,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		FILE_OPEN,
		FILE_NON_DIRECTORY_FILE,
		/* EaBuffer: */ nullptr,
		/* EaLength: */ 0);

	REQUIRE(NT_SUCCESS(status));

	return detail::unique_handle(handle);
}

TEST_CASE("Windows IOCP named pipe server and client", "[windows][iocp][pipe]")
{
	auto const iocp = win32::create_completion_port(1).value();

	auto const server_pipe = create_named_pipe();
	auto const client_pipe = connect_to_named_pipe(server_pipe.get());

	win32::set_completion_information(server_pipe.get(), iocp.get(), nullptr).value();
	win32::set_completion_information(client_pipe.get(), iocp.get(), nullptr).value();

	IO_STATUS_BLOCK r_io_status_block = {};
	unsigned char r_buf = 0;

	{
		NTSTATUS const status = win32::NtReadFile(
			server_pipe.get(),
			/* Event: */ NULL,
			/* ApcRoutine: */ nullptr,
			/* ApcContext: */ &r_io_status_block,
			&r_io_status_block,
			&r_buf,
			1,
			/* ByteOffset: */ nullptr,
			/* Key: */ nullptr);

		REQUIRE(status == STATUS_PENDING);
	}

	IO_STATUS_BLOCK w_io_status_block = {};
	unsigned char w_buf = 42;

	{
		NTSTATUS const status = win32::NtWriteFile(
			client_pipe.get(),
			/* Event: */ NULL,
			/* ApcRoutine: */ nullptr,
			/* ApcContext: */ &w_io_status_block,
			&w_io_status_block,
			&w_buf,
			1,
			/* ByteOffset: */ nullptr,
			/* Key: */ nullptr);

		REQUIRE(NT_SUCCESS(status));
	}

	{
		test::completion_storage<3> storage;
		REQUIRE(storage.remove(iocp, 3) == 2);

		REQUIRE(NT_SUCCESS(storage.get(r_io_status_block).IoStatusBlock.Status));
		REQUIRE(NT_SUCCESS(storage.get(w_io_status_block).IoStatusBlock.Status));
	}

	REQUIRE(r_buf == 42);
}

} // namespace
