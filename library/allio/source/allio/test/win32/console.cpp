#include <allio/detail/unique_handle.hpp>
//#include <allio/impl/win32/command_line.hpp>
//#include <allio/impl/win32/completion_port.hpp>
#include <allio/impl/win32/event.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/impl/win32/thread_event.hpp>
#include <allio/test/main.hpp>

#include <vsm/numeric.hpp>

#include <catch2/catch_all.hpp>

#include <format>
#include <iostream>

using namespace allio;
using namespace allio::win32;

namespace {

struct console_ioctl_buffer
{
	ULONG size;
	PVOID data;
};

template<size_t BufferCount>
struct console_ioctl_data
{
	HANDLE h_console;

	ULONG ctrl_buffer_count;
	ULONG data_buffer_count;

	console_ioctl_buffer buffers[BufferCount];
};

template<typename BufferType>
struct console_ioctl_info
{
	ULONG control_code;
	ULONG size;
	BufferType data;
};

struct console_write_console_info
{
	ULONG transfer_size;
	BOOLEAN is_wide;
};

static NTSTATUS write_console(
	HANDLE const h_console,
	void const* const text_data,
	size_t* const text_size,
	bool const is_wide)
{
	auto event = thread_event::get().value();

	console_ioctl_info<console_write_console_info> info =
	{
		.control_code = 0x01000006u,
		.size = sizeof(console_write_console_info),
		.data = { .is_wide = is_wide },
	};

	console_ioctl_data<3> data =
	{
		.h_console = NULL,
		.ctrl_buffer_count = 2,
		.data_buffer_count = 1,
		.buffers =
		{
			{
				.size = sizeof(info),
				.data = &info,
			},
			{
				.size = vsm::truncating(*text_size << static_cast<int>(is_wide)),
				.data = const_cast<PVOID>(text_data),
			},
			{
				.size = sizeof(info.data),
				.data = &info.data,
			},
		}
	};

	thread_event::io_status_block_t io_status_block;
	NTSTATUS status = win32::NtDeviceIoControlFile(
		h_console,
		event,
		/* ApcRoutine: */ nullptr,
		/* ApcContext: */ nullptr,
		&io_status_block,
		/* IoControlCode: */ 0x00500016u,
		&data,
		sizeof(data),
		/* OutputBuffer: */ nullptr,
		/* OutputBufferLength: */ 0);

	if (status == STATUS_PENDING)
	{
		status = event.wait_for_io(
			h_console,
			io_status_block,
			std::chrono::seconds(1));
	}

	*text_size = info.data.transfer_size >> static_cast<int>(is_wide);

	return status;
}

struct console_read_console_info
{
	WORD is_wide;
	WORD exe_name_length;
	ULONG control_characters_size;
	ULONG control_wake_up_mask;
	ULONG unknown_value_always_zero;
	ULONG transfer_size;
};

static NTSTATUS read_console(
	HANDLE const h_console,
	void* const text_data,
	size_t* const text_size,
	bool const is_wide)
{
	auto event = thread_event::get().value();

	// ExeNameBuffer[0].Buffer = ExeNameBufferStorage;
	// ExeNameBuffer[0].Length = 2 * ExeNameLength;
	// ExeNameBuffer[1].Buffer = Buffer;
	// ExeNameBuffer[1].Length = ControlCharactersCount;
	// DataBuffer.Buffer = Buffer;
	// DataBuffer.Length = BufferLengthBytes;
	// Status = ConsoleCallServerGeneric(
	//     FileHandle,
	//     0LL,
	//     (ConsoleControlInfo_Generic *)&ConsoleControl,
	//     0x1000005u,
	//     0x14u,
	//     ExeNameBuffer,
	//     2u,
	//     &DataBuffer,
	//     1u);

	console_ioctl_info<console_read_console_info> info =
	{
		.control_code = 0x01000005u,
		.size = sizeof(console_read_console_info),
		.data = {.is_wide = is_wide },
	};

	console_ioctl_data<5> data;
}


[[maybe_unused]]
static void throw_last_error(std::string_view const context)
{
	throw std::runtime_error(std::format("{}: 0x{:08x}", context, GetLastError()));
}

TEST_CASE("Windows async console", "[win32][console]")
{
	HANDLE const h_console = GetStdHandle(STD_OUTPUT_HANDLE);

	[[maybe_unused]] auto nt = GetProcAddress(GetModuleHandleW(L"NTDLL.DLL"), "NtDeviceIoControlFile");

	[[maybe_unused]] auto r1 = WriteConsoleW(
		h_console,
		L"TEST1",
		5,
		nullptr,
		nullptr);

	size_t text_size = 5;
	[[maybe_unused]] auto r2 = write_console(
		h_console,
		L"TEST2",
		&text_size,
		/* is_wide: */ true);

	[[maybe_unused]] int x = 0;

#if 0
	auto const child_process = []()
	{
		if (!AllocConsole())
		{
			throw_last_error("AllocConsole");
		}

		HANDLE h_console;
		HANDLE h_event;

	};

	win32::api_string_storage command_line_storage;
	wchar_t* const command_line = make_command_line(
		command_line_storage,
		test::get_child_executable_path().string(),
		test::make_child_args(child_process)).value();

	PROCESS_INFORMATION process_information;
	REQUIRE(CreateProcessW(
		/* lpApplicationName: */ nullptr,
		/* lpCommandLine: */ command_line,
		/* lpProcessAttributes: */ nullptr,
		/* lpThreadAttributes: */ nullptr,
		/* bInheritHandles: */ false,
		/* dwCreationFlags: */ DETACHED_PROCESS,
		/* lpEnvironment: */ nullptr,
		/* lpCurrentDirectory: */ nullptr,
		/* lpStartupInfo: */ nullptr,
		/* lpProcessInformation: */ &process_information));

	detail::unique_handle const h_thread(process_information.hThread);
	detail::unique_handle const h_process(process_information.hProcess);

	try
	{
		REQUIRE(win32::NtWaitForSingleObject(
			h_process.get(),
			/* Alertable: */ true,
			win32::kernel_timeout(std::chrono::seconds(1))) == STATUS_WAIT_0);

		DWORD child_exit_code;
		REQUIRE(GetExitCodeProcess(h_process.get(), &child_exit_code));
	}
	catch (...)
	{
		if (!TerminateProcess(h_process.get(), EXIT_FAILURE))
		{
			std::cerr << std::format("TerminateProcess: 0x{:08x}", GetLastError()) << std::endl;
		}

		throw;
	}
#endif
}

} // namespace
