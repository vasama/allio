#include <allio/impl/win32/wsa.hpp>

#include <vsm/numeric.hpp>

#pragma comment(lib, "ws2_32.lib")

using namespace allio;
using namespace allio::posix;
using namespace allio::win32;

namespace {

template<typename T>
static bool get_extension(SOCKET const socket, DWORD const io_control_code, GUID extension, T& out)
{
	DWORD bytes_returned;
	if (WSAIoctl(
		socket,
		io_control_code,
		&extension,
		sizeof(extension),
		&out,
		sizeof(out),
		&bytes_returned,
		/* lpOverlapped: */ nullptr,
		/* lpCompletionRoutine: */ nullptr) == SOCKET_ERROR)
	{
		return false;
	}
	vsm_assert(bytes_returned == sizeof(out));

	return true;
}

template<typename T>
static T* get_extension_ptr(SOCKET const socket, DWORD const io_control_code, GUID const& extension)
{
	T* ptr;
	return get_extension(socket, io_control_code, extension, ptr)
		? ptr
		: nullptr;
}

template<typename Pointer, auto ReturnValue>
struct wsa_unsupported;

template<typename R, typename... Ps, auto ReturnValue>
struct wsa_unsupported<R(*)(Ps...), ReturnValue>
{
	static R function(Ps...)
	{
		WSASetLastError(WSAEOPNOTSUPP);
		return ReturnValue;
	}
};

template<typename... Ps, auto ReturnValue>
struct wsa_unsupported<void(*)(Ps...), ReturnValue>
{
	static void function(Ps...)
	{
	}
};

template<INT ReturnValue, typename... Ps>
void set_extension(SOCKET const socket, INT(*&function)(Ps...), GUID const& extension)
{
	auto const f = get_extension_ptr<INT(Ps...)>(
		socket,
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		extension);

	function = f != nullptr
		? f
		: wsa_unsupported<INT(*)(Ps...), ReturnValue>::function;
}

template<auto ReturnValue, typename Pointer>
void set_unsupported(Pointer& pointer)
{
	pointer = wsa_unsupported<Pointer, ReturnValue>::function;
}

struct wsa_owner
{
	socket_error const error;

	wsa_owner(socket_error const error)
		: error(error)
	{
	}

	~wsa_owner()
	{
		if (error == socket_error::none)
		{
			WSACleanup();
		}
	}
};

} // namespace

static LPFN_ACCEPTEX accept_ex;
static LPFN_CONNECTEX connect_ex;
static LPFN_WSASENDMSG send_msg;
static LPFN_WSARECVMSG recv_msg;
static RIO_EXTENSION_FUNCTION_TABLE rio;

static socket_error _wsa_init()
{
	WSADATA data;
	if (int const e = WSAStartup(MAKEWORD(2, 2), &data))
	{
		return static_cast<socket_error>(e);
	}

	SOCKET const socket = WSASocketW(
		AF_INET,
		SOCK_STREAM,
		IPPROTO_TCP,
		/* lpProtocolInfo: */ nullptr,
		/* group: */ NULL,
		/* dwFlags: */ 0);

	bool got_rio = false;

	if (socket != SOCKET_ERROR)
	{
		set_extension<FALSE>(socket, accept_ex, WSAID_ACCEPTEX);
		set_extension<FALSE>(socket, connect_ex, WSAID_CONNECTEX);
		set_extension<SOCKET_ERROR>(socket, send_msg, WSAID_WSASENDMSG);
		set_extension<SOCKET_ERROR>(socket, recv_msg, WSAID_WSARECVMSG);

		got_rio = get_extension(
			socket,
			SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER,
			WSAID_MULTIPLE_RIO,
			rio);
	}

	if (!got_rio)
	{
		set_unsupported<FALSE>(rio.RIOReceive);
		set_unsupported<FALSE>(rio.RIOReceiveEx);
		set_unsupported<FALSE>(rio.RIOSend);
		set_unsupported<FALSE>(rio.RIOSendEx);
		set_unsupported<0>(rio.RIOCloseCompletionQueue);
		set_unsupported<RIO_INVALID_CQ>(rio.RIOCreateCompletionQueue);
		set_unsupported<RIO_INVALID_RQ>(rio.RIOCreateRequestQueue);
		set_unsupported<RIO_CORRUPT_CQ>(rio.RIODequeueCompletion);
		set_unsupported<0>(rio.RIODeregisterBuffer);
		set_unsupported<WSAEOPNOTSUPP>(rio.RIONotify);
		set_unsupported<RIO_INVALID_BUFFERID>(rio.RIORegisterBuffer);
		set_unsupported<FALSE>(rio.RIOResizeCompletionQueue);
		set_unsupported<FALSE>(rio.RIOResizeRequestQueue);
	}

	closesocket(socket);

	return socket_error::none;
}

//TODO: Automatically initialize WSA like the kernel APIs.
vsm::result<void> win32::wsa_init()
{
	static wsa_owner const e = _wsa_init();

	if (e.error != socket_error::none)
	{
		return vsm::unexpected(e.error);
	}

	return {};
}


DWORD win32::wsa_accept_ex(
	SOCKET const listen_socket,
	SOCKET const accept_socket,
	wsa_accept_address_buffer& address,
	OVERLAPPED& overlapped)
{
	using address_buffer = wsa_accept_address_buffer;
	static_assert(
		offsetof(address_buffer, local) == 0 &&
		offsetof(address_buffer, remote) == sizeof(address_buffer::local),
		"AcceptEx writes the local and remote addresses into the same buffer, one after the other.");

	DWORD transferred = static_cast<DWORD>(-1);
	if (!accept_ex(
		listen_socket,
		accept_socket,
		/* lpOutputBuffer: */ &address,
		/* dwReceiveDataLength: */ 0,
		sizeof(address.local),
		sizeof(address.remote),
		&transferred,
		&overlapped))
	{
		return WSAGetLastError();
	}
	vsm_assert(transferred == 0);
	return 0;
}

DWORD win32::wsa_connect_ex(
	SOCKET const socket,
	socket_address const& addr,
	OVERLAPPED& overlapped)
{
	DWORD transferred = static_cast<DWORD>(-1);
	if (!connect_ex(
		socket,
		&addr.addr,
		addr.size,
		/* lpSendBuffer: */ nullptr,
		/* dwSendDataLength: */ 0,
		&transferred,
		&overlapped))
	{
		return WSAGetLastError();
	}
	vsm_assert(transferred == 0);
	return 0;
}


static DWORD wsa_send_msg(
	SOCKET const socket,
	LPWSAMSG const message,
	LPDWORD const transferred,
	LPOVERLAPPED const overlapped)
{
	if (send_msg(
		socket,
		message,
		/* dwFlags: */ 0,
		transferred,
		overlapped,
		/* lpCompletionRoutine: */ nullptr) == socket_error_value)
	{
		return WSAGetLastError();
	}
	return 0;
}

static DWORD wsa_recv_msg(
	SOCKET const socket,
	LPWSAMSG const message,
	LPDWORD const transferred,
	LPOVERLAPPED const overlapped)
{
	if (recv_msg(
		socket,
		message,
		transferred,
		overlapped,
		/* lpCompletionRoutine: */ nullptr) == socket_error_value)
	{
		return WSAGetLastError();
	}
	return 0;
}


DWORD win32::wsa_send_msg(
	SOCKET const socket,
	LPWSAMSG const message,
	LPDWORD const transferred)
{
	return wsa_send_msg(socket, message, transferred, nullptr);
}

DWORD win32::wsa_send_msg(
	SOCKET const socket,
	LPWSAMSG const message,
	LPOVERLAPPED const overlapped)
{
	return wsa_send_msg(socket, message, nullptr, overlapped);
}

DWORD win32::wsa_recv_msg(
	SOCKET const socket,
	LPWSAMSG const message,
	LPDWORD const transferred)
{
	return wsa_recv_msg(socket, message, transferred, nullptr);
}

DWORD win32::wsa_recv_msg(
	SOCKET const socket,
	LPWSAMSG const message,
	LPOVERLAPPED const overlapped)
{
	return wsa_recv_msg(socket, message, nullptr, overlapped);
}


void win32::rio_close_completion_queue(RIO_CQ const rq)
{
	rio.RIOCloseCompletionQueue(rq);
}

void win32::rio_deregister_buffer(RIO_BUFFERID const buffer_id)
{
	rio.RIODeregisterBuffer(buffer_id);
}

vsm::result<unique_rio_cq> win32::rio_create_completion_queue(
	size_t const queue_size,
	HANDLE const completion_port,
	void* const completion_key,
	OVERLAPPED& overlapped)
{
	RIO_NOTIFICATION_COMPLETION completion;
	RIO_NOTIFICATION_COMPLETION* p_completion = nullptr;

	if (completion_port != NULL)
	{
		completion.Type = RIO_IOCP_COMPLETION;
		completion.Iocp.IocpHandle = completion_port;
		completion.Iocp.CompletionKey = completion_key;
		completion.Iocp.Overlapped = &overlapped;
		p_completion = &completion;
	}

	RIO_CQ const cq = rio.RIOCreateCompletionQueue(
		queue_size,
		p_completion);

	if (cq == RIO_INVALID_CQ)
	{
		return vsm::unexpected(posix::get_last_socket_error());
	}

	return vsm_lazy(unique_rio_cq(cq));
}

vsm::result<RIO_RQ> win32::rio_create_request_queue(
	SOCKET const socket,
	size_t const max_outstanding_receive,
	size_t const max_receive_data_buffers,
	size_t const max_outstanding_send,
	size_t const max_send_data_buffers,
	RIO_CQ const receive_cq,
	RIO_CQ const send_cq,
	void* const socket_context)
{
	RIO_RQ const rq = rio.RIOCreateRequestQueue(
		socket,
		max_outstanding_receive,
		max_receive_data_buffers,
		max_outstanding_send,
		max_send_data_buffers,
		receive_cq,
		send_cq,
		socket_context);

	if (rq == RIO_INVALID_RQ)
	{
		return vsm::unexpected(posix::get_last_socket_error());
	}

	return rq;
}

vsm::result<unique_rio_buffer> win32::rio_register_buffer(
	std::span<std::byte> const buffer)
{
	RIO_BUFFERID const buffer_id = rio.RIORegisterBuffer(
		reinterpret_cast<PCHAR>(buffer.data()),
		vsm::saturating(buffer.size()));

	if (buffer_id == RIO_INVALID_BUFFERID)
	{
		return vsm::unexpected(posix::get_last_socket_error());
	}

	return vsm_lazy(unique_rio_buffer(buffer_id));
}

size_t win32::rio_dequeue_completion(
	RIO_CQ const cq,
	std::span<RIORESULT> const buffer)
{
	auto const size = rio.RIODequeueCompletion(
		cq,
		buffer.data(),
		vsm::saturating(buffer.size()));
	vsm_assert(size != RIO_CORRUPT_CQ);
	return size;
}
