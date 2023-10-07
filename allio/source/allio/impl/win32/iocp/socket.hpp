#pragma once

#include <allio/error.hpp>
#include <allio/impl/posix/socket.hpp>
#include <allio/impl/win32/error.hpp>

#include <win32.h>

namespace allio::win32 {

inline void cancel_socket_io(SOCKET const socket, OVERLAPPED& overlapped)
{
	if (!CancelIoEx((HANDLE)socket, &overlapped))
	{
		if (DWORD const error = GetLastError(); error != ERROR_NOT_FOUND)
		{
			detail::unrecoverable_error(static_cast<system_error>(error));
		}
	}
}

} // namespace allio::win32
