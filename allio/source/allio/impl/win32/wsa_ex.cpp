#include <allio/impl/win32/wsa_ex.hpp>

#include <MSWSock.h>

using namespace allio;
using namespace allio::posix;
using namespace allio::win32;

namespace {

template<typename Pointer, GUID Extension>
struct wsa_extension;

template<typename... Ps, GUID Extension>
struct wsa_extension<BOOL(*)(SOCKET, Ps...), Extension>
{
	static BOOL invoke(SOCKET const socket, Ps const... args)
	{
		typedef BOOL function_type(SOCKET, Ps...);

		static function_type* const function = [&]()
		{
			GUID guid = Extension;

			function_type* function;
			DWORD bytes_returned;

			if (!WSAIoctl(
				socket,
				SIO_GET_EXTENSION_FUNCTION_POINTER,
				&guid,
				sizeof(guid),
				&function,
				sizeof(function),
				&bytes_returned,
				/* lpOverlapped: */ nullptr,
				/* lpCompletionRoutine: */ nullptr))
			{
				vsm_assert(bytes_returned == sizeof(function));
			}
			else
			{
				function = nullptr;
			}

			return function;
		}();

		if (function == nullptr)
		{
			WSASetLastError(WSAEOPNOTSUPP);
			return FALSE;
		}

		return function(socket, args...);
	}
};

} // namespace


DWORD win32::wsa_accept_ex(
	SOCKET const listen_socket,
	SOCKET const accept_socket,
	wsa_accept_address_buffer& address,
	OVERLAPPED& overlapped)
{
	using ext = wsa_extension<LPFN_ACCEPTEX, GUID WSAID_ACCEPTEX>;

	using address_buffer = wsa_accept_address_buffer;
	static_assert(
		offsetof(address_buffer, local) == 0 &&
		offsetof(address_buffer, remote) == sizeof(address_buffer::local),
		"AcceptEx writes the local and remote addresses into the same buffer, one after the other.");

	DWORD transferred = static_cast<DWORD>(-1);
	if (!ext::invoke(
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
	using ext = wsa_extension<LPFN_CONNECTEX, GUID WSAID_CONNECTEX>;

	DWORD transferred = static_cast<DWORD>(-1);
	if (!ext::invoke(
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
	using ext = wsa_extension<LPFN_WSASENDMSG, GUID WSAID_WSASENDMSG>;
	if (ext::invoke(
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
	using ext = wsa_extension<LPFN_WSARECVMSG, GUID WSAID_WSARECVMSG>;
	if (ext::invoke(
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
