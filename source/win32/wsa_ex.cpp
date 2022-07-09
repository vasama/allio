#include "wsa_ex.hpp"

#include <MSWSock.h>

using namespace allio;
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
				nullptr,
				nullptr))
			{
				allio_ASSERT(bytes_returned == sizeof(function));
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
	LPOVERLAPPED const overlapped)
{
	DWORD bytes_received = static_cast<DWORD>(-1);
	if (wsa_extension<LPFN_ACCEPTEX, GUID WSAID_ACCEPTEX>::invoke(
		listen_socket,
		accept_socket,
		&address,
		0,
		sizeof(address.local),
		sizeof(address.remote),
		&bytes_received,
		overlapped))
	{
		allio_ASSERT(bytes_received == 0);
		return 0;
	}
	return WSAGetLastError();
}

DWORD win32::wsa_connect_ex(
	SOCKET const socket,
	socket_address const& addr,
	LPOVERLAPPED const overlapped)
{
	DWORD bytes_sent = static_cast<DWORD>(-1);
	if (wsa_extension<LPFN_CONNECTEX, GUID WSAID_CONNECTEX>::invoke(
		socket,
		&addr.addr,
		addr.size,
		nullptr,
		0,
		&bytes_sent,
		overlapped))
	{
		allio_ASSERT(bytes_sent == 0);
		return 0;
	}
	return WSAGetLastError();
}
