#include <allio/impl/win32/kernel.hpp>

#include <allio/detail/dynamic_buffer.hpp>
#include <allio/detail/unique_handle.hpp>
#include <allio/impl/posix/socket.hpp>
#include <allio/impl/win32/wsa.hpp>

#include <allio/test/spawn.hpp>

#include <vsm/numeric.hpp>

#include <catch2/catch_all.hpp>

#include <mstcpip.h>
#include <WS2tcpip.h>

#pragma comment(lib, "fwpuclnt.lib")

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

static constexpr DWORD AFD_ENDPOINT_CONNECTIONLESS          = 0x00000001;
static constexpr DWORD AFD_ENDPOINT_MESSAGE_ORIENTED        = 0x00000010;
static constexpr DWORD AFD_ENDPOINT_RAW                     = 0x00000100;

static constexpr ULONG IOCTL_AFD_BIND                       = 0x00012003;
static constexpr ULONG IOCTL_AFD_CONNECT                    = 0x00012007;
static constexpr ULONG IOCTL_AFD_START_LISTEN               = 0x0001200B;
static constexpr ULONG IOCTL_AFD_WAIT_FOR_LISTEN            = 0x0001200C;
static constexpr ULONG IOCTL_AFD_ACCEPT                     = 0x00012010;
static constexpr ULONG IOCTL_AFD_SELECT                     = 0x00012024;
static constexpr ULONG IOCTL_AFD_SET_CONTEXT                = 0x00012047;
static constexpr ULONG IOCTL_AFD_SET_INFORMATION            = 0x000120BF;

static constexpr ULONG AFD_SHARE_UNIQUE                     = 0;
static constexpr ULONG AFD_SHARE_REUSE                      = 1;
static constexpr ULONG AFD_SHARE_WILDCARD                   = 2;
static constexpr ULONG AFD_SHARE_EXCLUSIVE                  = 3;

static constexpr ULONG AFD_SET_INFORMATION_SETSOCKOPT       = 1;
static constexpr ULONG AFD_SET_INFORMATION_IOCTL            = 3;

struct _AFD_POLL_HANDLE_DATA
{
	HANDLE Handle;
	ULONG Events;
}
typedef AFD_POLL_HANDLE_DATA, *PAFD_POLL_HANDLE_DATA;

struct _AFD_POLL_DATA
{
	LARGE_INTEGER Timeout;
	ULONG HandleCount;
	BOOLEAN UseSAN;
	AFD_POLL_HANDLE_DATA Handles;
}
typedef AFD_POLL_DATA, *PAFD_POLL_DATA;

struct _AFD_CREATE_PACKET
{
	DWORD EndpointFlags;
	DWORD GroupID;
	DWORD AddressFamily;
	DWORD SocketType;
	DWORD Protocol;
	DWORD SizeOfTransportName;
	WCHAR TransportName[1];
}
typedef AFD_CREATE_PACKET, *PAFD_CREATE_PACKET;

struct _AFD_BIND_DATA
{
	ULONG ShareType;
	SOCKADDR Address[];
}
typedef AFD_BIND_DATA, *PAFD_BIND_DATA;

struct _AFD_CONNECT_INFO
{
	BOOLEAN UseSAN;
	ULONG_PTR Root;
	ULONG_PTR Reserved;
	SOCKADDR Address[];
}
typedef AFD_CONNECT_INFO, *PAFD_CONNECT_INFO;

struct _AFD_LISTEN_DATA
{
	BOOLEAN UseSAN;
	ULONG Backlog;
	BOOLEAN UseDelayedAcceptance;
}
typedef AFD_LISTEN_DATA, *PAFD_LISTEN_DATA;

struct _AFD_RECEIVED_ACCEPT_DATA
{
	ULONG SequenceNumber;
	SOCKADDR Address[];
}
typedef AFD_RECEIVED_ACCEPT_DATA, *PAFD_RECEIVED_ACCEPT_DATA;

struct _AFD_ACCEPT_DATA
{
	ULONG UseSAN;
	ULONG SequenceNumber;
	HANDLE ListenHandle;
}
typedef AFD_ACCEPT_DATA, *PAFD_ACCEPT_DATA;

struct _AFD_SET_INFORMATION_DATA
{
	ULONG Unknown1;
	ULONG Level;
	ULONG Option;
	BOOLEAN Unknown2;
	PVOID Value;
	LONGLONG ValueSize;
}
typedef AFD_SET_INFORMATION_DATA, *PAFD_SET_INFORMATION_DATA;


static HANDLE g_socket_filter;

static NTSTATUS NTAPI NtCreateFile_hook(
	PHANDLE            FileHandle,
	ACCESS_MASK        DesiredAccess,
	POBJECT_ATTRIBUTES ObjectAttributes,
	PIO_STATUS_BLOCK   IoStatusBlock,
	PLARGE_INTEGER     AllocationSize,
	ULONG              FileAttributes,
	ULONG              ShareAccess,
	ULONG              CreateDisposition,
	ULONG              CreateOptions,
	PVOID              EaBuffer,
	ULONG              EaLength)
{
	FILE_FULL_EA_INFORMATION* ea = (FILE_FULL_EA_INFORMATION*)EaBuffer;
	AFD_CREATE_PACKET* acp = (AFD_CREATE_PACKET*)(ea->EaName + ea->EaNameLength + 1);

	NTSTATUS s = win32::NtCreateFile(
		FileHandle,
		DesiredAccess,
		ObjectAttributes,
		IoStatusBlock,
		AllocationSize,
		FileAttributes,
		ShareAccess,
		CreateDisposition,
		CreateOptions,
		EaBuffer,
		EaLength);

	printf("%08x %02u/%02u/%03u %08x '%.*ls'\n",
		(unsigned)(uintptr_t)FileHandle,
		acp->AddressFamily,
		acp->SocketType,
		acp->Protocol,
		acp->EndpointFlags,
		(int)acp->SizeOfTransportName,
		acp->TransportName);

	return s;
}

static NTSTATUS NTAPI NtDeviceIoControlFile_hook(
	HANDLE FileHandle,
	HANDLE Event,
	PIO_APC_ROUTINE ApcRoutine,
	PVOID ApcContext,
	PIO_STATUS_BLOCK IoStatusBlock,
	ULONG IoControlCode,
	PVOID InputBuffer,
	ULONG InputBufferLength,
	PVOID OutputBuffer,
	ULONG OutputBufferLength)
{
	if (FileHandle == g_socket_filter)
	{
		posix::socket_address_union addr[2];
		int addr_count = 0;

		switch (IoControlCode)
		{
		case IOCTL_AFD_SET_CONTEXT:
		case IOCTL_AFD_SELECT:
			break;
		case IOCTL_AFD_BIND:
			{
				[[maybe_unused]] AFD_BIND_DATA* p = (AFD_BIND_DATA*)InputBuffer;
				memcpy(&addr[addr_count++], p->Address, InputBufferLength - sizeof(AFD_BIND_DATA));
				[[maybe_unused]] int x = 0;
			}
			break;
		case IOCTL_AFD_CONNECT:
			{
				[[maybe_unused]] AFD_CONNECT_INFO* p = (AFD_CONNECT_INFO*)InputBuffer;
				[[maybe_unused]] int x = 0;
			}
			break;
		case IOCTL_AFD_START_LISTEN:
			{
				[[maybe_unused]] AFD_LISTEN_DATA* p = (AFD_LISTEN_DATA*)InputBuffer;
				[[maybe_unused]] int x = 0;
			}
			break;
		case IOCTL_AFD_WAIT_FOR_LISTEN:
			{
				[[maybe_unused]] int x = 0;
			}
			break;
		case IOCTL_AFD_ACCEPT:
			{
				[[maybe_unused]] AFD_ACCEPT_DATA* p = (AFD_ACCEPT_DATA*)InputBuffer;
				[[maybe_unused]] int x = 0;
			}
			break;
		case IOCTL_AFD_SET_INFORMATION:
			{
				[[maybe_unused]] AFD_SET_INFORMATION_DATA* p = (AFD_SET_INFORMATION_DATA*)InputBuffer;
				[[maybe_unused]] int x = 0;

				switch (p->Option)
				{
				case SIO_SET_SECURITY:
					{
						[[maybe_unused]] SOCKET_SECURITY_SETTINGS* q = (SOCKET_SECURITY_SETTINGS*)p->Value;
						[[maybe_unused]] int y = 0;
					}
					break;
				}
			}
			break;
		default:
			{
				[[maybe_unused]] int x = 0;
			}
			break;
		}

		for (int i = 0; i < addr_count; ++i)
		{
			[[maybe_unused]] auto const& a = addr[i];
			printf("  %08x %05x %04u %04u\n",
				(unsigned)(uintptr_t)FileHandle,
				IoControlCode,
				InputBufferLength,
				OutputBufferLength);
		}
	}

	NTSTATUS s = win32::NtDeviceIoControlFile(
		FileHandle,
		Event,
		ApcRoutine,
		ApcContext,
		IoStatusBlock,
		IoControlCode,
		InputBuffer,
		InputBufferLength,
		OutputBuffer,
		OutputBufferLength);

	return s;
}

static void set_socket_security(SOCKET const socket)
{
	SOCKET_SECURITY_SETTINGS settings = {};
	settings.SecurityProtocol = SOCKET_SECURITY_PROTOCOL_IPSEC;
	settings.SecurityFlags = SOCKET_SETTINGS_ALLOW_INSECURE;

	REQUIRE(WSASetSocketSecurity(
		socket,
		&settings,
		sizeof(settings),
		/* Overlapped: */ nullptr,
		/* CompletionRoutine:*/ nullptr) != SOCKET_ERROR);

	[[maybe_unused]] int x = 0;
}

TEST_CASE("AFD-TEST")
{
	LoadLibraryW(L"MSWSOCK.DLL");

	static bool dummy = []()
	{
		HANDLE const mswsock = GetModuleHandleW(L"MSWSOCK.DLL");

		[[maybe_unused]] uintptr_t* const pp_NtCreateFile =
			reinterpret_cast<uintptr_t*>(
				reinterpret_cast<char*>(mswsock) + 0x549D0);

		[[maybe_unused]] uintptr_t const p_NtCreateFile_hook =
			reinterpret_cast<uintptr_t>(NtCreateFile_hook);

		[[maybe_unused]] uintptr_t* const pp_NtDeviceControlIoFile =
			reinterpret_cast<uintptr_t*>(
				reinterpret_cast<char*>(mswsock) + 0x54990);

		[[maybe_unused]] uintptr_t const p_NtDeviceControlIoFile_hook =
			reinterpret_cast<uintptr_t>(NtDeviceIoControlFile_hook);

		return true;
	}();

#if 0
	int const address_family = GENERATE(AF_UNIX, AF_INET, AF_INET6);

	int const type = address_family == AF_UNIX
		? SOCK_STREAM
		: GENERATE(SOCK_STREAM, SOCK_DGRAM, SOCK_RAW);

	int const protocol = [&]() -> int
	{
		if (address_family != AF_UNIX)
		{
			switch (type)
			{
			case SOCK_STREAM:
				return IPPROTO_TCP;

			case SOCK_DGRAM:
				return IPPROTO_UDP;

			case SOCK_RAW:
				return IPPROTO_RAW;
			}
		}
		return 0;
	}();
#endif
	
	int address_family = AF_INET;
	int type = SOCK_STREAM;
	int protocol = IPPROTO_TCP;

	auto const addr = posix::socket_address::make(
		ipv4_endpoint(ipv4_address::localhost, 0x1234)).value();

	auto const server_future = test::spawn([&]()
	{
		auto [socket, flags] = posix::create_socket(
			address_family,
			type,
			protocol,
			io_flags::none).value();
		g_socket_filter = (HANDLE)socket.get();

		set_socket_security(socket.get());

		posix::socket_listen(socket.get(), addr, 1).value();

		(void)posix::socket_poll(
			socket.get(),
			posix::socket_poll_r,
			deadline::never()).value();

		posix::socket_address accept_addr;
		auto [s, f] = posix::socket_accept(
			socket.get(),
			accept_addr,
			deadline::never(),
			io_flags::none).value();

		char b;
		recv(s.get(), &b, 1, 0);
	});
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	auto [socket, flags] = posix::create_socket(
		address_family,
		type,
		protocol,
		io_flags::none).value();
	//g_socket_filter = (HANDLE)socket.get();

	posix::socket_connect(
		socket.get(),
		addr,
		deadline::never()).value();
}
