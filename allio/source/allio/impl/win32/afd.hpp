#pragma once

#include <allio/impl/win32/kernel.hpp>

#include <WinSock2.h>
#include <ws2ipdef.h>
#include <afunix.h>

namespace allio::win32 {

static constexpr ULONG AFD_ENDPOINT_CONNECTIONLESS          = 0x00000001;
static constexpr ULONG AFD_ENDPOINT_MESSAGE_ORIENTED        = 0x00000010;
static constexpr ULONG AFD_ENDPOINT_RAW                     = 0x00000100;

static constexpr ULONG AFD_SHARE_UNIQUE                     = 0;
static constexpr ULONG AFD_SHARE_REUSE                      = 1;
static constexpr ULONG AFD_SHARE_WILDCARD                   = 2;
static constexpr ULONG AFD_SHARE_EXCLUSIVE                  = 3;

static constexpr ULONG AFD_EVENT_RECEIVE                    = 0x0001;
static constexpr ULONG AFD_EVENT_OOB_RECEIVE                = 0x0002;
static constexpr ULONG AFD_EVENT_SEND                       = 0x0004;
static constexpr ULONG AFD_EVENT_DISCONNECT                 = 0x0008;
static constexpr ULONG AFD_EVENT_ABORT                      = 0x0010;
static constexpr ULONG AFD_EVENT_CLOSE                      = 0x0020;
static constexpr ULONG AFD_EVENT_CONNECT                    = 0x0040;
static constexpr ULONG AFD_EVENT_ACCEPT                     = 0x0080;
static constexpr ULONG AFD_EVENT_CONNECT_FAIL               = 0x0100;
static constexpr ULONG AFD_EVENT_QOS                        = 0x0200;
static constexpr ULONG AFD_EVENT_GROUP_QOS                  = 0x0400;
static constexpr ULONG AFD_EVENT_ROUTING_INTERFACE_CHANGE   = 0x0800;
static constexpr ULONG AFD_EVENT_ADDRESS_LIST_CHANGE        = 0x1000;
static constexpr ULONG AFD_EVENT_ALL                        = 0x1FFF;

struct _AFD_SELECT_HANDLE
{
	HANDLE Handle;
	ULONG Events;
	NTSTATUS Status;
}
typedef AFD_SELECT_HANDLE, *PAFD_SELECT_HANDLE;

struct _AFD_SELECT_INFO
{
	LARGE_INTEGER Timeout;
	ULONG HandleCount;
	BOOL Exclusive;
}
typedef AFD_SELECT_INFO, *PAFD_SELECT_INFO;

struct _AFD_SELECT_INFO_1
{
	AFD_SELECT_INFO Info;
	AFD_SELECT_HANDLE Handle;
}
typedef AFD_SELECT_INFO_1, *PAFD_SELECT_INFO_1;

struct _AFD_RECEIVED_ACCEPT_DATA
{
	ULONG SequenceNumber;
}
typedef AFD_RECEIVED_ACCEPT_DATA, *PAFD_RECEIVED_ACCEPT_DATA;

struct _AFD_RECV_INFO
{
	WSABUF Buffers;
	ULONG BufferCount;
	ULONG AfdFlags;
	ULONG TdiFlags;
}
typedef AFD_RECV_INFO, *PAFD_RECV_INFO;

struct _AFD_SEND_INFO
{
	WSABUF Buffers;
	ULONG BufferCount;
	ULONG AfdFlags;
	ULONG TdiFlags;
}
typedef AFD_SEND_INFO, *PAFD_SEND_INFO;


NTSTATUS AfdCreateSocket(
	_Out_ PHANDLE SocketHandle,
	_In_ ULONG EndpointFlags,
	_In_ GROUP GroupID,
	_In_ ULONG AddressFamily,
	_In_ ULONG SocketType,
	_In_ ULONG Protocol,
	_In_ ULONG CreateOptions,
	_In_opt_ PUNICODE_STRING TransportName);

void AfdInitializeSelect1(
	_Out_ PAFD_SELECT_INFO_1 Info,
	_In_ HANDLE SocketHandle,
	_In_ ULONG Events,
	_In_ BOOLEAN Exclusive,
	_In_ PLARGE_INTEGER Timeout);

NTSTATUS AfdSelect(
	_In_ HANDLE SocketHandle,
	_In_opt_ HANDLE Event,
	_In_opt_ PIO_APC_ROUTINE ApcRoutine,
	_In_opt_ PVOID ApcContext,
	_Out_ PIO_STATUS_BLOCK IoStatusBlock,
	_In_ PAFD_SELECT_INFO Info);

NTSTATUS AfdSetSockOpt(
	_In_ HANDLE SocketHandle,
	_In_ ULONG Level,
	_In_ ULONG Option,
	_In_ PVOID Value,
	_In_ ULONG ValueLength);

NTSTATUS AfdBind(
	_In_ HANDLE SocketHandle,
	_In_ ULONG ShareType,
	_In_ SOCKADDR const* Address,
	_In_ ULONG AddressLength);

NTSTATUS AfdListen(
	_In_ HANDLE SocketHandle,
	_In_ ULONG Backlog);

NTSTATUS AfdWaitForAccept(
	_In_ HANDLE SocketHandle,
	_In_opt_ HANDLE Event,
	_In_opt_ PIO_APC_ROUTINE ApcRoutine,
	_In_opt_ PVOID ApcContext,
	_Out_ PIO_STATUS_BLOCK IoStatusBlock,
	_Out_writes_bytes_(BufferLength) PAFD_RECEIVED_ACCEPT_DATA Buffer,
	_In_ ULONG BufferLength);

NTSTATUS AfdAccept(
	_In_ HANDLE SocketHandle,
	_In_opt_ HANDLE Event,
	_In_opt_ PIO_APC_ROUTINE ApcRoutine,
	_In_opt_ PVOID ApcContext,
	_Out_ PIO_STATUS_BLOCK IoStatusBlock,
	_In_ HANDLE AcceptHandle,
	_In_ ULONG SequenceNumber);

NTSTATUS AfdConnect(
	_In_ HANDLE SocketHandle,
	_In_opt_ HANDLE Event,
	_In_opt_ PIO_APC_ROUTINE ApcRoutine,
	_In_opt_ PVOID ApcContext,
	_Out_ PIO_STATUS_BLOCK IoStatusBlock,
	_In_ SOCKADDR const* Address,
	_In_ ULONG AddressLength);

NTSTATUS AfdRecv(
	_In_ HANDLE SocketHandle,
	_In_opt_ HANDLE Event,
	_In_opt_ PIO_APC_ROUTINE ApcRoutine,
	_In_opt_ PVOID ApcContext,
	_Out_ PIO_STATUS_BLOCK IoStatusBlock,
	_In_ PAFD_RECV_INFO Info);

NTSTATUS AfdSend(
	_In_ HANDLE SocketHandle,
	_In_opt_ HANDLE Event,
	_In_opt_ PIO_APC_ROUTINE ApcRoutine,
	_In_opt_ PVOID ApcContext,
	_Out_ PIO_STATUS_BLOCK IoStatusBlock,
	_In_ PAFD_SEND_INFO Info);

} // namespace allio::win32
