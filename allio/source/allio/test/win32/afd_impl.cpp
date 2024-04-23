#include <allio/impl/win32/afd.hpp>

#include <winioctl.h>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

namespace {

#define AFD_CTL_CODE(Function, Method) \
	CTL_CODE(0, (FILE_DEVICE_NETWORK << 10 | (Function)), (Method), 0)

static constexpr ULONG IOCTL_AFD_BIND                       = AFD_CTL_CODE(0x00, METHOD_NEITHER);
static constexpr ULONG IOCTL_AFD_CONNECT                    = AFD_CTL_CODE(0x01, METHOD_NEITHER);
static constexpr ULONG IOCTL_AFD_START_LISTEN               = AFD_CTL_CODE(0x02, METHOD_NEITHER);
static constexpr ULONG IOCTL_AFD_WAIT_FOR_LISTEN            = AFD_CTL_CODE(0x03, METHOD_BUFFERED);
static constexpr ULONG IOCTL_AFD_ACCEPT                     = AFD_CTL_CODE(0x04, METHOD_BUFFERED);
static constexpr ULONG IOCTL_AFD_RECV                       = AFD_CTL_CODE(0x05, METHOD_NEITHER);
static constexpr ULONG IOCTL_AFD_RECV_DATAGRAM              = AFD_CTL_CODE(0x06, METHOD_NEITHER);
static constexpr ULONG IOCTL_AFD_SEND                       = AFD_CTL_CODE(0x07, METHOD_NEITHER);
static constexpr ULONG IOCTL_AFD_SEND_DATAGRAM              = AFD_CTL_CODE(0x08, METHOD_NEITHER);
static constexpr ULONG IOCTL_AFD_SELECT                     = AFD_CTL_CODE(0x09, METHOD_BUFFERED);
static constexpr ULONG IOCTL_AFD_SET_INFORMATION            = AFD_CTL_CODE(0x2F, METHOD_NEITHER);

static constexpr ULONG AFD_SET_INFORMATION_SETSOCKOPT       = 1;
static constexpr ULONG AFD_SET_INFORMATION_IOCTL            = 3;

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

struct _AFD_CONNECT_DATA
{
	BOOLEAN UseSAN;
	ULONG_PTR Root;
	ULONG_PTR Reserved;
	SOCKADDR Address[];
}
typedef AFD_CONNECT_DATA, *PAFD_CONNECT_DATA;

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
	BOOLEAN UseSAN;
	ULONG SequenceNumber;
	HANDLE ListenHandle;
}
typedef AFD_ACCEPT_DATA, *PAFD_ACCEPT_DATA;

struct _AFD_SET_INFORMATION_DATA
{
	ULONG Mode;
	union
	{
		struct
		{
			ULONG Level;
			ULONG Option;
		}
		SetSockOpt;

		struct
		{
			ULONG Reserved;
			ULONG ControlCode;
		}
		Ioctl;
	};
	BOOLEAN Boolean;
	PVOID Data;
	LONGLONG DataLength;
}
typedef AFD_SET_INFORMATION_DATA, *PAFD_SET_INFORMATION_DATA;

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
	BOOLEAN Unknown;
}
typedef AFD_POLL_DATA, *PAFD_POLL_DATA;

static constexpr size_t MaxAddressLength = sizeof(SOCKADDR_UNION);
static constexpr size_t MaxTransportNameLength = 256;

} // namespace

NTSTATUS win32::AfdCreateSocket(
	_Out_ PHANDLE const SocketHandle,
	_In_ ULONG const EndpointFlags,
	_In_ GROUP const GroupID,
	_In_ ULONG const AddressFamily,
	_In_ ULONG const SocketType,
	_In_ ULONG const Protocol,
	_In_ DWORD const CreateOptions,
	_In_opt_ PUNICODE_STRING TransportName)
{
	UNICODE_STRING DefaultTransportName = {};

	if (TransportName == nullptr)
	{
		TransportName = &DefaultTransportName;
	}
	else if (TransportName->Length > MaxTransportNameLength)
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
	EaInformation->EaNameLength = sizeof(CommandName) - 1;
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

void win32::AfdInitializeSelect1(
	_Out_ PAFD_SELECT_INFO_1 const Info,
	_In_ HANDLE const SocketHandle,
	_In_ ULONG const Events,
	_In_ BOOLEAN const Exclusive,
	_In_ PLARGE_INTEGER const Timeout)
{
	Info->Info.Timeout.QuadPart = Timeout != nullptr
		? Timeout->QuadPart
		: std::numeric_limits<decltype(LARGE_INTEGER::QuadPart)>::max();
	Info->Info.HandleCount = 1;
	Info->Info.Exclusive = Exclusive;
	Info->Handle.Handle = SocketHandle;
	Info->Handle.Events = Events;
	Info->Handle.Status = STATUS_PENDING;
}

NTSTATUS win32::AfdSelect(
	_In_ HANDLE const SocketHandle,
	_In_opt_ HANDLE const Event,
	_In_opt_ PIO_APC_ROUTINE const ApcRoutine,
	_In_opt_ PVOID const ApcContext,
	_Out_ PIO_STATUS_BLOCK const IoStatusBlock,
	_In_ PAFD_SELECT_INFO const Info)
{
	ULONG const BufferSize =
		sizeof(AFD_SELECT_INFO) +
		Info->HandleCount * sizeof(AFD_SELECT_HANDLE);

	return win32::NtDeviceIoControlFile(
		SocketHandle,
		Event,
		ApcRoutine,
		ApcContext,
		IoStatusBlock,
		IOCTL_AFD_SELECT,
		Info,
		BufferSize,
		Info,
		BufferSize);
}

#if 0
NTSTATUS win32::AfdSetSockOpt(
	_In_ HANDLE const SocketHandle,
	_In_ ULONG const Level,
	_In_ ULONG const Option,
	_In_ PVOID const Value,
	_In_ ULONG const ValueLength)
{
	AFD_SET_INFORMATION_DATA Data =
	{
		.Unknown1 = 1,
		.Level = Level,
		.Option = Option,
		.Unknown2 = 1,
		.Data = Value,
		.DataLength = ValueLength,
	};

	IO_STATUS_BLOCK IoStatusBlock;

	NTSTATUS Status = win32::NtDeviceIoControlFile(
		SocketHandle,
		/* Event: */ NULL,
		/* ApcRoutine: */ nullptr,
		/* ApcContext: */ nullptr,
		&IoStatusBlock,
		IOCTL_AFD_SETSOCKOPT,
		&Data,
		sizeof(Data),
		/* OutputBuffer: */ nullptr,
		/* OutputBufferLength: */ 0);

	if (Status == STATUS_PENDING)
	{
		//TODO: Wait
	}

	return Status;
}
#endif

NTSTATUS win32::AfdBind(
	_In_ HANDLE const SocketHandle,
	_In_ ULONG const ShareType,
	_In_ SOCKADDR const* const Address,
	_In_ ULONG const AddressLength)
{
	if (AddressLength > MaxAddressLength)
	{
		return STATUS_INVALID_PARAMETER;
	}

	alignas(AFD_BIND_DATA) UCHAR Buffer[sizeof(AFD_BIND_DATA) + MaxAddressLength];
	PAFD_BIND_DATA const Data = new (Buffer) AFD_BIND_DATA;

	Data->ShareType = ShareType;
	memcpy(Data->Address, Address, AddressLength);

	IO_STATUS_BLOCK IoStatusBlock;

	NTSTATUS Status = win32::NtDeviceIoControlFile(
		SocketHandle,
		/* Event: */ NULL,
		/* ApcRoutine: */ nullptr,
		/* ApcContext: */ nullptr,
		&IoStatusBlock,
		IOCTL_AFD_BIND,
		Data,
		sizeof(*Data) + AddressLength,
		Data->Address,
		MaxAddressLength);

	if (Status == STATUS_PENDING)
	{
		//TODO: Wait
	}

	return Status;
}

NTSTATUS win32::AfdListen(
	_In_ HANDLE const SocketHandle,
	_In_ ULONG const Backlog)
{
	AFD_LISTEN_DATA Data;
	Data.UseSAN = FALSE;
	Data.Backlog = Backlog;
	Data.UseDelayedAcceptance = FALSE;

	IO_STATUS_BLOCK IoStatusBlock;

	return win32::NtDeviceIoControlFile(
		SocketHandle,
		/* Event: */ NULL,
		/* ApcRoutine: */ nullptr,
		/* ApcContext: */ nullptr,
		&IoStatusBlock,
		IOCTL_AFD_START_LISTEN,
		&Data,
		sizeof(Data),
		/* OutputBuffer: */ nullptr,
		/* OutputBufferLength: */ 0);
}

NTSTATUS win32::AfdWaitForAccept(
	_In_ HANDLE const SocketHandle,
	_In_opt_ HANDLE const Event,
	_In_opt_ PIO_APC_ROUTINE const ApcRoutine,
	_In_opt_ PVOID const ApcContext,
	_Out_ PIO_STATUS_BLOCK const IoStatusBlock,
	_Out_writes_bytes_(BufferLength) PAFD_RECEIVED_ACCEPT_DATA const Buffer,
	_In_ ULONG const BufferLength)
{
	if (Buffer == nullptr)
	{
		return STATUS_INVALID_PARAMETER;
	}

	if (BufferLength < sizeof(*Buffer))
	{
		return STATUS_BUFFER_TOO_SMALL;
	}

	return win32::NtDeviceIoControlFile(
		SocketHandle,
		Event,
		ApcRoutine,
		ApcContext,
		IoStatusBlock,
		IOCTL_AFD_WAIT_FOR_LISTEN,
		/* InputBuffer: */ nullptr,
		/* InputBufferLength: */ 0,
		Buffer,
		BufferLength);
}

NTSTATUS win32::AfdAccept(
	_In_ HANDLE const SocketHandle,
	_In_opt_ HANDLE const Event,
	_In_opt_ PIO_APC_ROUTINE const ApcRoutine,
	_In_opt_ PVOID const ApcContext,
	_Out_ PIO_STATUS_BLOCK const IoStatusBlock,
	_In_ HANDLE const AcceptHandle,
	_In_ ULONG const SequenceNumber)
{
	AFD_ACCEPT_DATA Data =
	{
		.UseSAN = FALSE,
		.SequenceNumber = SequenceNumber,
		.ListenHandle = AcceptHandle,
	};

	return win32::NtDeviceIoControlFile(
		SocketHandle,
		Event,
		ApcRoutine,
		ApcContext,
		IoStatusBlock,
		IOCTL_AFD_ACCEPT,
		&Data,
		sizeof(Data),
		/* OutputBuffer: */ nullptr,
		/* OutputBufferLength: */ 0);
}

NTSTATUS win32::AfdConnect(
	_In_ HANDLE const SocketHandle,
	_In_opt_ HANDLE const Event,
	_In_opt_ PIO_APC_ROUTINE const ApcRoutine,
	_In_opt_ PVOID const ApcContext,
	_Out_ PIO_STATUS_BLOCK const IoStatusBlock,
	_In_ SOCKADDR const* const Address,
	_In_ ULONG const AddressLength)
{
	if (AddressLength > MaxAddressLength)
	{
		return STATUS_INVALID_ADDRESS;
	}

	alignas(AFD_CONNECT_DATA) UCHAR Buffer[sizeof(AFD_CONNECT_DATA) + MaxAddressLength];
	PAFD_CONNECT_DATA const Data = new (Buffer) AFD_CONNECT_DATA;

	Data->UseSAN = FALSE;
	Data->Root = 0;
	memcpy(Data->Address, Address, AddressLength);

	return win32::NtDeviceIoControlFile(
		SocketHandle,
		Event,
		ApcRoutine,
		ApcContext,
		IoStatusBlock,
		IOCTL_AFD_CONNECT,
		Data,
		sizeof(*Data) + AddressLength,
		/* OutputBuffer: */ nullptr,
		/* OutputBufferLength: */ 0);
}

NTSTATUS win32::AfdRecv(
	_In_ HANDLE const SocketHandle,
	_In_opt_ HANDLE const Event,
	_In_opt_ PIO_APC_ROUTINE const ApcRoutine,
	_In_opt_ PVOID const ApcContext,
	_Out_ PIO_STATUS_BLOCK const IoStatusBlock,
	_In_ PAFD_RECV_INFO const Info)
{
	return win32::NtDeviceIoControlFile(
		SocketHandle,
		Event,
		ApcRoutine,
		ApcContext,
		IoStatusBlock,
		IOCTL_AFD_RECV,
		Info,
		sizeof(AFD_RECV_INFO),
		/* OutputBuffer: */ nullptr,
		/* OutputBufferLength: */ 0);
}

NTSTATUS win32::AfdSend(
	_In_ HANDLE const SocketHandle,
	_In_opt_ HANDLE const Event,
	_In_opt_ PIO_APC_ROUTINE const ApcRoutine,
	_In_opt_ PVOID const ApcContext,
	_Out_ PIO_STATUS_BLOCK const IoStatusBlock,
	_In_ PAFD_SEND_INFO const Info)
{
	return win32::NtDeviceIoControlFile(
		SocketHandle,
		Event,
		ApcRoutine,
		ApcContext,
		IoStatusBlock,
		IOCTL_AFD_SEND,
		Info,
		sizeof(AFD_SEND_INFO),
		/* OutputBuffer: */ nullptr,
		/* OutputBufferLength: */ 0);
}
