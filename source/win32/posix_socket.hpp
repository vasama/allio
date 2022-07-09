#pragma once

#ifndef allio_detail_SOCKET_API
#	error Include source/posix_socket.hpp instead.
#endif

#include <allio/win32/platform.hpp>

#ifndef _WINSOCKAPI_
#	define _WINSOCKAPI_
#endif

#include "wsa_error.hpp"

#include <win32.h>
#include <WinSock2.h>
#include <ws2ipdef.h>
#include <afunix.h>

namespace allio {

using socket_type = SOCKET;
using socket_address_size_type = int;

static constexpr socket_type invalid_socket = INVALID_SOCKET;
static constexpr size_t unix_socket_max_path = UNIX_PATH_MAX;


inline std::error_code get_last_socket_error()
{
	return std::error_code(WSAGetLastError(), win32::wsa_error_category::get());
}

inline native_platform_handle wrap_socket(SOCKET const socket)
{
	return win32::wrap_handle<SOCKET>(socket);
}

inline SOCKET unwrap_socket(native_platform_handle const socket)
{
	return win32::unwrap_handle<SOCKET>(socket);
}


inline result<void> close_socket(socket_type const socket)
{
	if (closesocket(socket))
	{
		return allio_ERROR(get_last_socket_error());
	}
	return {};
}

} // namespace allio
