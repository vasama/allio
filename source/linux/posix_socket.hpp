#pragma once

#ifndef allio_detail_SOCKET_API
#	error Include source/posix_socket.hpp instead.
#endif

#include <allio/linux/detail/undef.i>

#include <allio/linux/platform.hpp>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>

#include <allio/linux/detail/undef.i>

namespace allio {

using socket_type = int;
using socket_address_size_type = unsigned int;

static constexpr socket_type invalid_socket = static_cast<socket_type>(-1);
static constexpr size_t unix_socket_max_path = sizeof(sockaddr_un::sun_path);


inline std::error_code get_last_socket_error()
{
	return std::error_code(errno, std::system_category());
}

inline native_platform_handle wrap_socket(socket_type const socket)
{
	return linux::wrap_handle(socket);
}

inline socket_type unwrap_socket(native_platform_handle const socket)
{
	return linux::unwrap_handle(socket);
}


inline result<void> close_socket(socket_type const socket)
{
	if (close(socket))
	{
		return allio_ERROR(get_last_socket_error());
	}
	return {};
}

} // namespace allio
