//TODO: Move to posix/

#pragma once

#ifndef allio_detail_socket_api
#	error Include <allio/impl/posix/socket.hpp> instead.
#endif

#include <allio/linux/detail/undef.i>

#include <allio/detail/unique_handle.hpp>
#include <allio/impl/error_encoding.hpp>
#include <allio/impl/linux/error.hpp>

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

inline constexpr int socket_disconnect_r = SHUT_RD;
inline constexpr int socket_disconnect_w = SHUT_WR;
inline constexpr int socket_disconnect_rw = SHUT_RDWR;


using socket_error = linux::system_error;

[[nodiscard]] inline socket_error get_last_socket_error()
{
	return linux::get_last_error();
}


[[nodiscard]] inline detail::native_platform_handle wrap_socket(socket_type const socket)
{
	return detail::wrap_handle(socket);
}

[[nodiscard]] inline socket_type unwrap_socket(detail::native_platform_handle const socket)
{
	return detail::unwrap_handle(socket);
}


inline void close_socket(socket_type const socket)
{
	detail::close_platform_handle(socket);
}

using unique_socket = detail::unique_handle;

} // namespace allio::posix
