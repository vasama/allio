#pragma once

#include <allio/win32/detail/iocp/multiplexer.hpp>

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

vsm::result<bool> submit_socket_io(
	detail::iocp_multiplexer const& m,
	detail::native_handle<detail::platform_object_t> const& h,
	auto&& callable)
{
	// Check for synchronous completion support before submitting the operation.
	// After the operation is submitted, it is no longer safe to access the handle.
	bool const supports_synchronous_completion = m.supports_synchronous_completion(h);

	// If using a multithreaded completion port, after this call
	// another thread will race to complete this operation.
	DWORD const error = vsm_forward(callable)();

	if (error == WSA_IO_PENDING)
	{
		return false;
	}

	if (error != 0)
	{
		return vsm::unexpected(static_cast<posix::socket_error>(error));
	}

	return supports_synchronous_completion;
}

} // namespace allio::win32
