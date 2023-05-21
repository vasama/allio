#include <allio/impl/posix_socket.hpp>

using namespace allio;

vsm::result<unique_socket> allio::create_socket(int const address_family, flags const handle_flags)
{
	int protocol = 0;

	switch (address_family)
	{
	case AF_INET:
	case AF_INET6:
		protocol = IPPROTO_TCP;
		break;
	}

	socket_type const socket = ::socket(address_family, SOCK_STREAM | SOCK_CLOEXEC, protocol);

	if (socket == invalid_socket)
	{
		return std::unexpected(get_last_socket_error());
	}

	return vsm::result<unique_socket>(vsm::result_value, socket);
}

vsm::result<unique_socket> allio::accept_socket(socket_type const listen_socket, socket_address& addr, socket_parameters const& create_args)
{
	socket_type const socket = ::accept(socket, &addr.addr, &addr.size);

	if (socket == invalid_socket)
	{
		return std::unexpected(get_last_socket_error());
	}

	return vsm::result<unique_socket>(vsm::result_value, socket);
}
