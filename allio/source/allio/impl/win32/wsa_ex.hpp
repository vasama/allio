#pragma once

#include <allio/impl/posix/socket.hpp>

namespace allio::win32 {

struct wsa_accept_address_buffer
{
	class socket_address_buffer : public posix::socket_address_union
	{
		// AcceptEx requires an extra 16 bytes.
		std::byte m_dummy_buffer[16];
	};
	static_assert(alignof(socket_address_buffer) <= sizeof(socket_address_buffer));

	socket_address_buffer local;
	socket_address_buffer remote;
};

DWORD wsa_accept_ex(
	SOCKET socket_listen,
	SOCKET socket_accept,
	wsa_accept_address_buffer& address,
	OVERLAPPED& overlapped);

DWORD wsa_connect_ex(
	SOCKET socket,
	posix::socket_address const& addr,
	OVERLAPPED& overlapped);

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
