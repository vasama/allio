#if 0
#include <allio/impl/win32/kernel.hpp>

#include <allio/detail/dynamic_buffer.hpp>
#include <allio/detail/unique_handle.hpp>
#include <allio/impl/posix/socket.hpp>
#include <allio/impl/win32/wsa.hpp>

#include <allio/test/spawn.hpp>

#include <vsm/numeric.hpp>

#include <catch2/catch_all.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

static constexpr DWORD AFD_ENDPOINT_CONNECTIONLESS          = 0x00000001;
static constexpr DWORD AFD_ENDPOINT_MESSAGE_ORIENTED        = 0x00000010;
static constexpr DWORD AFD_ENDPOINT_RAW                     = 0x00000100;

static constexpr ULONG IOCTL_AFD_BIND                       = 0x00012003;
static constexpr ULONG IOCTL_AFD_CONNECT                    = 0x00012007;
static constexpr ULONG IOCTL_AFD_ACCEPT                     = 0x0001200C;
static constexpr ULONG IOCTL_AFD_START_LISTEN               = 0x0001200B;
static constexpr ULONG IOCTL_AFD_WAIT_FOR_LISTEN            = 0x00012010;

static constexpr ULONG AFD_SHARE_UNIQUE                     = 0;
static constexpr ULONG AFD_SHARE_REUSE                      = 1;
static constexpr ULONG AFD_SHARE_WILDCARD                   = 2;
static constexpr ULONG AFD_SHARE_EXCLUSIVE                  = 3;

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

#if 1
struct SOCKADDR_UNION
{
	union
	{
		SOCKADDR Sockaddr;
		SOCKADDR_IN SockaddrIn;
		SOCKADDR_IN6 SockaddrIn6;
		SOCKADDR_UN SockaddrUn;
	};
};

static constexpr size_t MaxAddressLength = sizeof(SOCKADDR_UNION);
static constexpr size_t MaxTransportNameLength = 256;

namespace {

template<typename PREFIX>
union AFD_STORAGE
{
	UCHAR Storage[sizeof(PREFIX) + MaxAddressLength];
};

} // namespace

static NTSTATUS GetAddressExtents(
	INT const AddressFamily,
	PULONG const MinAddressLength,
	PULONG const MaxAddressLength)
{
	switch (AddressFamily)
	{
	case AF_UNIX:
		*MinAddressLength = sizeof(SOCKADDR_UN);
		*MaxAddressLength = sizeof(SOCKADDR_UN);
		return STATUS_SUCCESS;

	case AF_INET:
		*MinAddressLength = sizeof(SOCKADDR_IN);
		*MaxAddressLength = sizeof(SOCKADDR_IN);
		return STATUS_SUCCESS;

	case AF_INET6:
		*MinAddressLength = sizeof(SOCKADDR_IN6);
		*MaxAddressLength = sizeof(SOCKADDR_IN6);
		return STATUS_SUCCESS;
	}
	return STATUS_INVALID_ADDRESS;
}

static NTSTATUS AfdSocketCreate(
	PHANDLE const SocketHandle,
	ULONG const EndpointFlags,
	GROUP const GroupID,
	ULONG const AddressFamily,
	ULONG const SocketType,
	ULONG const Protocol,
	DWORD const CreateOptions,
	PUNICODE_STRING const TransportName)
{
	if (TransportName->Length > MaxTransportNameLength)
	{
		return STATUS_INVALID_PARAMETER;
	}

	static constexpr wchar_t DeviceNameData[] = L"\\Device\\Afd\\Endpoint";
	static constexpr char CommandName[] = "AfdOpenPacketXX";

	UNICODE_STRING DeviceName;
	DeviceName.Buffer = const_cast<wchar_t*>(DeviceNameData);
	DeviceName.Length = sizeof(DeviceNameData);
	DeviceName.MaximumLength = DeviceName.Length;

	OBJECT_ATTRIBUTES ObjectAttributes = {};
	ObjectAttributes.Length = sizeof(ObjectAttributes);
	ObjectAttributes.ObjectName = &DeviceName;

	static constexpr size_t EaBufferLength =
		sizeof(FILE_FULL_EA_INFORMATION) + sizeof(DeviceNameData) +
		sizeof(AFD_CREATE_PACKET) + MaxTransportNameLength;

	UCHAR EaBuffer[EaBufferLength];
	PFILE_FULL_EA_INFORMATION const EaInformation = new (EaBuffer) FILE_FULL_EA_INFORMATION;
	EaInformation->NextEntryOffset = 0;
	EaInformation->Flags = 0;
	EaInformation->EaNameLength = sizeof(CommandName);
	EaInformation->EaValueLength = sizeof(AFD_CREATE_PACKET) + TransportName->Length;
	memcpy(EaInformation->EaName, CommandName, sizeof(CommandName));

	PVOID const PacketBuffer = EaInformation->EaName + sizeof(CommandName);
	PAFD_CREATE_PACKET const CreatePacket = new (PacketBuffer) AFD_CREATE_PACKET;
	CreatePacket->EndpointFlags = EndpointFlags;
	CreatePacket->GroupID = GroupID;
	CreatePacket->AddressFamily = AddressFamily;
	CreatePacket->SocketType = SocketType;
	CreatePacket->Protocol = Protocol;
	CreatePacket->SizeOfTransportName = TransportName->Length;
	memcpy(CreatePacket->TransportName, TransportName->Buffer, TransportName->Length);

	IO_STATUS_BLOCK IoStatusBlock;
	NTSTATUS const status = win32::NtCreateFile(
		SocketHandle,
		GENERIC_READ | GENERIC_WRITE | SYNCHRONIZE,
		&ObjectAttributes,
		&IoStatusBlock,
		/* AllocationSize: */ nullptr,
		/* FileAttributes: */ 0,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		FILE_OPEN_IF,
		CreateOptions,
		EaBuffer,
		EaBufferLength);

	vsm_assert(status != STATUS_PENDING);

	return status;
}

static NTSTATUS AfdSocketBind(
	HANDLE const SocketHandle,
	PIO_STATUS_BLOCK const IoStatusBlock,
	ULONG const ShareType,
	SOCKADDR const* const Address,
	ULONG const AddressLength)
{
	if (AddressLength > MaxAddressLength)
	{
		return STATUS_INVALID_PARAMETER;
	}

	AFD_STORAGE<AFD_BIND_DATA> Storage;
	PAFD_BIND_DATA const Data = new (Storage.Storage) AFD_BIND_DATA;

	Data->ShareType = ShareType;
	memcpy(Data->Address, Address, AddressLength);

	return win32::NtDeviceIoControlFile(
		SocketHandle,
		/* Event: */ NULL,
		/* ApcRoutine: */ nullptr,
		/* ApcContext: */ nullptr,
		IoStatusBlock,
		IOCTL_AFD_BIND,
		Data,
		sizeof(*Data) + AddressLength,
		/* OutputBuffer: */ nullptr,
		/* OutputBufferLength: */ 0);
}

static NTSTATUS AfdSocketConnect(
	HANDLE const SocketHandle,
	PIO_STATUS_BLOCK const IoStatusBlock,
	SOCKADDR const* const Address,
	ULONG const AddressLength)
{
	if (AddressLength > MaxAddressLength)
	{
		return STATUS_INVALID_ADDRESS;
	}

	AFD_STORAGE<AFD_CONNECT_INFO> Storage;
	PAFD_CONNECT_INFO const Data = new (Storage.Storage) AFD_CONNECT_INFO;

	Data->UseSAN = FALSE;
	Data->Root = 0;
	memcpy(Data->Address, Address, AddressLength);

	return win32::NtDeviceIoControlFile(
		SocketHandle,
		/* Event: */ NULL,
		/* ApcRoutine: */ nullptr,
		/* ApcContext: */ nullptr,
		IoStatusBlock,
		IOCTL_AFD_CONNECT,
		Data,
		sizeof(*Data) + AddressLength,
		/* OutputBuffer: */ nullptr,
		/* OutputBufferLength: */ 0);
}

static NTSTATUS AfdSocketListen(
	HANDLE const SocketHandle,
	PIO_STATUS_BLOCK const IoStatusBlock,
	ULONG const Backlog)
{
	AFD_LISTEN_DATA Data;
	Data.UseSAN = FALSE;
	Data.Backlog = Backlog;
	Data.UseDelayedAcceptance = FALSE;

	return win32::NtDeviceIoControlFile(
		SocketHandle,
		/* Event: */ NULL,
		/* ApcRoutine: */ nullptr,
		/* ApcContext: */ nullptr,
		IoStatusBlock,
		IOCTL_AFD_CONNECT,
		&Data,
		sizeof(Data),
		/* OutputBuffer: */ nullptr,
		/* OutputBufferLength: */ 0);
}

static NTSTATUS AfdSocketWaitForAccept(
	HANDLE const SocketHandle,
	PIO_STATUS_BLOCK const IoStatusBlock,
	PVOID const OutputBuffer,
	ULONG const OutputBufferLength)
{
	return win32::NtDeviceIoControlFile(
		SocketHandle,
		/* Event: */ NULL,
		/* ApcRoutine: */ nullptr,
		/* ApcContext: */ nullptr,
		IoStatusBlock,
		IOCTL_AFD_WAIT_FOR_LISTEN,
		/* InputBuffer: */ nullptr,
		/* InputBufferLength: */ 0,
		OutputBuffer,
		OutputBufferLength);
}

static NTSTATUS AfdSocketAcceptAddress(
	PVOID const OutputBuffer,
	ULONG const OutputBufferLength,
	PULONG const SequenceNumber,
	PSOCKADDR const Address,
	PULONG const AddressLength)
{
	PAFD_RECEIVED_ACCEPT_DATA const Data =
		reinterpret_cast<PAFD_RECEIVED_ACCEPT_DATA>(OutputBuffer);

	ULONG const DataAddressLength = GetAddressLength(Data->Address);

	if (AddressLength < DataAddressLength)
	{
		return STATUS_BUFFER_TOO_SMALL;
	}

	*SequenceNumber = Data->SequenceNumber;
	*AddressLength = DataAddressLength;

	memmove(Address, Data->Address, DataAddressLength);

	return STATUS_SUCCESS;
}

static NTSTATUS AfdSocketConfirmAccept(
	HANDLE const SocketHandle,
	PIO_STATUS_BLOCK const IoStatusBlock,
	ULONG const SequenceNumber,
	HANDLE const AcceptSocketHandle)
{
	AFD_ACCEPT_DATA Data;
	Data.UseSAN = FALSE;
	Data.SequenceNumber = SequenceNumber;
	Data.ListenHandle = AcceptSocketHandle;

	return win32::NtDeviceIoControlFile(
		SocketHandle,
		/* Event: */ NULL,
		/* ApcRoutine: */ nullptr,
		/* ApcContext: */ nullptr,
		IoStatusBlock,
		IOCTL_AFD_ACCEPT,
		&Data,
		sizeof(Data),
		/* OutputBuffer: */ nullptr,
		/* OutputBufferLength: */ 0);
}


static vsm::result<unique_handle> afd_socket(
	int const address_family,
	int const type,
	int const protocol,
	posix::socket_flags const flags)
{
	ULONG endpoint_flags = 0;
	ULONG create_options = 0;

	if (vsm::any_flags(flags, posix::socket_flags::multiplexable))
	{
	}
	else
	{
		create_options |= FILE_SYNCHRONOUS_IO_NONALERT;
	}

	if (vsm::any_flags(flags, posix::socket_flags::registered_io))
	{
		//TODO
	}

	UNICODE_STRING transport_name = {};

	HANDLE handle;
	NTSTATUS const status = AfdSocketCreate(
		&handle,
		endpoint_flags,
		/* GroupID: */ 0,
		static_cast<ULONG>(address_family),
		static_cast<ULONG>(type),
		static_cast<ULONG>(protocol),
		create_options,
		&transport_name);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return vsm_lazy(unique_handle(handle));
}

static vsm::result<void> afd_bind(
	HANDLE const handle,
	posix::socket_address const& addr)
{
	afd_storage<AFD_BIND_DATA> storage;

	IO_STATUS_BLOCK io_status_block;
	NTSTATUS status = AfdSocketBind(
		handle,
		&io_status_block,
		new (storage.storage) AFD_BIND_DATA,
		sizeof(storage),
		AFD_SHARE_WILDCARD, //TODO
		&addr.addr,
		static_cast<ULONG>(addr.size));

	if (status == STATUS_PENDING)
	{
		status = io_wait(handle, &io_status_block, deadline::never());
	}

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return {};
}

static vsm::result<void> afd_listen(
	HANDLE const handle,
	uint32_t const backlog)
{
	afd_storage<AFD_LISTEN_DATA> storage;

	IO_STATUS_BLOCK io_status_block;
	NTSTATUS status = AfdSocketListen(
		handle,
		&io_status_block,
		new (storage.storage) AFD_LISTEN_DATA,
		sizeof(storage),
		backlog);

	if (status == STATUS_PENDING)
	{
		status = io_wait(handle, &io_status_block, deadline::never());
	}

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return {};
}

static vsm::result<unique_handle> afd_accept(
	HANDLE const listen_handle,
	HANDLE const accept_handle,
	posix::socket_address& addr)
{
	ULONG sequence_number;
	{
		afd_storage<AFD_RECEIVED_ACCEPT_DATA> storage;
		auto const data = new (storage.storage) AFD_RECEIVED_ACCEPT_DATA;

		IO_STATUS_BLOCK io_status_block;
		NTSTATUS status = AfdSocketWaitAccept(
			listen_handle,
			&io_status_block,
			data,
			sizeof(storage));

		if (status == STATUS_PENDING)
		{
			status = io_wait(listen_handle, &io_status_block, deadline::never());
		}

		if (!NT_SUCCESS(status))
		{
			return vsm::unexpected(static_cast<kernel_error>(status));
		}

		sequence_number = data->SequenceNumber;
		//TODO: Handle the size correctly
		//memcpy(addr.addr, data->Address, addr.size);
	}

	{
		afd_storage<AFD_ACCEPT_DATA> storage;
		auto const data = new(storage.storage) AFD_ACCEPT_DATA;

		IO_STATUS_BLOCK io_status_block;
		NTSTATUS status = AfdSocketReadAccept(
			listen_handle,
			&io_status_block,
			data,
			sizeof(storage),
			sequence_number,
			accept_handle);

		if (status == STATUS_PENDING)
		{
			status = io_wait(listen_handle, &io_status_block, deadline::never());
		}

		if (!NT_SUCCESS(status))
		{
			return vsm::unexpected(static_cast<kernel_error>(status));
		}
	}

	return {};
}

static vsm::result<void> afd_connect(
	HANDLE const handle,
	posix::socket_address const& addr)
{
	afd_storage<AFD_CONNECT_INFO> storage;
	auto const data = new(storage.storage) AFD_CONNECT_INFO;

	IO_STATUS_BLOCK io_status_block;
	NTSTATUS status = AfdSocketConnect(
		handle,
		&io_status_block,
		data,
		sizeof(storage),
		&addr.addr,
		addr.size);

	if (status == STATUS_PENDING)
	{
		status = io_wait(handle, &io_status_block, deadline::never());
	}

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return {};
}
#endif


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
		(unsigned)FileHandle,
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
		case IOCTL_AFD_BIND:
			{
				AFD_BIND_DATA* p = (AFD_BIND_DATA*)InputBuffer;
				int x = 0;
			}
			break;
		case IOCTL_AFD_CONNECT:
			{
				AFD_CONNECT_INFO* p = (AFD_CONNECT_INFO*)InputBuffer;
				int x = 0;
			}
			break;
		}

		for (int i = 0; i < addr_count; ++i)
		{
			auto const& a = addr[i];
			printf("  %08x %05x %04u %04u\n",
				(unsigned)FileHandle,
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

TEST_CASE("AFD", "[windows][afd]")
{
	kernel_init().value();
	wsa_init().value();

#if 0
#endif

	static bool dummy = []()
	{
		HANDLE const mswsock = GetModuleHandleW(L"MSWSOCK.DLL");

		uintptr_t* const pp_NtCreateFile = reinterpret_cast<uintptr_t*>(
			reinterpret_cast<char*>(mswsock) + 0x549D0);
		uintptr_t const p_NtCreateFile_hook = reinterpret_cast<uintptr_t>(NtCreateFile_hook);

		uintptr_t* const pp_NtDeviceControlIoFile = reinterpret_cast<uintptr_t*>(
			reinterpret_cast<char*>(mswsock) + 0x54990);
		uintptr_t const p_NtDeviceControlIoFile_hook = reinterpret_cast<uintptr_t>(NtDeviceIoControlFile_hook);

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
		//ipv6_endpoint(ipv6_address::localhost, 0x1234)).value();

#if 1
	UNICODE_STRING transport_name;
	transport_name.Buffer = nullptr;
	transport_name.Length = 0;
	transport_name.MaximumLength = 0;

	auto const server_future = test::spawn([&]()
	{
		HANDLE listen_h;
		NTSTATUS const status = AfdSocketCreate(
			&listen_h,
			0,
			0,
			AF_INET,
			SOCK_STREAM,
			IPPROTO_TCP,
			0,
			&transport_name);
		REQUIRE(NT_SUCCESS(status));



		posix::socket_listen(socket.get(), addr, nullptr).value();

		posix::socket_address accept_addr;
		auto [s, f] = posix::socket_accept(
			socket.get(),
			accept_addr,
			deadline::never()).value();

		char b;
		recv(s.get(), &b, 1, 0);
	});
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	HANDLE afd_h;
	{
		NTSTATUS const status = AfdSocketCreate(
			&afd_h,
			0,
			0,
			AF_INET,
			SOCK_STREAM,
			IPPROTO_TCP,
			0,
			&transport_name);
		REQUIRE(NT_SUCCESS(status));
	}

	{
		posix::socket_address_union zero_addr;
		memset(&zero_addr, 0, sizeof(zero_addr));
		zero_addr.addr.sa_family = addr.addr.sa_family;

		IO_STATUS_BLOCK io_status_block = make_io_status_block();
		NTSTATUS status = AfdSocketBind(
			afd_h,
			&io_status_block,
			&zero_addr.addr,
			addr.size,
			AFD_SHARE_WILDCARD);

		if (status == STATUS_PENDING)
		{
			status = io_wait(afd_h, &io_status_block, deadline::never());
		}

		REQUIRE(NT_SUCCESS(status));
	}

	{
		IO_STATUS_BLOCK io_status_block = make_io_status_block();
		NTSTATUS status = AfdSocketConnect(
			afd_h,
			&io_status_block,
			&addr.addr,
			addr.size);

		if (status == STATUS_PENDING)
		{
			status = io_wait(afd_h, &io_status_block, deadline::never());
		}

		REQUIRE(NT_SUCCESS(status));
	}

	int x = 0;
#else
	auto const server_future = test::spawn([&]()
	{
		auto [socket, flags] = posix::create_socket(
			address_family,
			type,
			protocol,
			posix::socket_flags::none).value();

		posix::socket_listen(socket.get(), addr, nullptr).value();

		posix::socket_address accept_addr;
		auto [s, f] = posix::socket_accept(
			socket.get(),
			accept_addr,
			deadline::never()).value();

		char b;
		recv(s.get(), &b, 1, 0);
	});
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	auto [socket, flags] = posix::create_socket(
		address_family,
		type,
		protocol,
		posix::socket_flags::none).value();
	g_socket_filter = (HANDLE)socket.get();

	posix::socket_connect(
		socket.get(),
		addr,
		deadline::never()).value();
#endif
}
#endif
