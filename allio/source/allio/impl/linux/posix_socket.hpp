#pragma once
//TODO: Move to posix/

#ifndef allio_detail_socket_api
#	error Include <allio/impl/posix/socket.hpp> instead.
#endif

#include <allio/linux/detail/undef.i>

#include <allio/impl/linux/error.hpp>
#include <allio/linux/platform.hpp>

#include <poll.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>

#include <allio/linux/detail/undef.i>

namespace allio::posix {

using socket_type = int;
using socket_address_size_type = unsigned int;

inline constexpr auto socket_error_value = -1;
inline constexpr socket_type invalid_socket = static_cast<socket_type>(-1);
inline constexpr size_t unix_socket_max_path = sizeof(sockaddr_un::sun_path);

using socket_poll_mask = short;
inline constexpr socket_poll_mask socket_poll_r = POLLIN;
inline constexpr socket_poll_mask socket_poll_w = POLLOUT;


using socket_error = linux::system_error;

inline socket_error get_last_socket_error()
{
	return linux::get_last_error();
}


inline native_platform_handle wrap_socket(socket_type const socket)
{
	return linux::wrap_handle(socket);
}

inline socket_type unwrap_socket(native_platform_handle const socket)
{
	return linux::unwrap_handle(socket);
}


inline vsm::result<void> close_socket(socket_type const socket)
{
	if (close(socket))
	{
		return vsm::unexpected(get_last_socket_error());
	}
	return {};
}

} // namespace allio::posix
