#pragma once

#include <allio/impl/posix_socket.hpp>

namespace allio::win32 {

struct wsa_accept_address_buffer
{
	class socket_address_buffer : public socket_address_union
	{
		// AcceptEx requires an extra 16 bytes.
		std::byte m_dummy_buffer[16];
	};
	static_assert(alignof(socket_address_buffer) <= sizeof(socket_address_buffer));

	socket_address_buffer local;
	socket_address_buffer remote;
};

DWORD wsa_accept_ex(
	SOCKET listen_socket,
	SOCKET accept_socket,
	wsa_accept_address_buffer& address,
	LPOVERLAPPED overlapped);

DWORD wsa_connect_ex(
	SOCKET socket,
	socket_address const& addr,
	LPOVERLAPPED overlapped);

DWORD wsa_send_msg(
	SOCKET socket,
	LPWSAMSG message,
	LPDWORD transferred);

DWORD wsa_send_msg(
	SOCKET socket,
	LPWSAMSG message,
	LPOVERLAPPED overlapped);

DWORD wsa_recv_msg(
	SOCKET socket,
	LPWSAMSG message,
	LPDWORD transferred);

DWORD wsa_recv_msg(
	SOCKET socket,
	LPWSAMSG message,
	LPOVERLAPPED overlapped);

} // namespace allio::win32
