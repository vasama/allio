#include <allio/impl/posix_socket.hpp>

using namespace allio;

vsm::result<unique_socket_with_flags> allio::create_socket(int const address_family, common_socket_handle::create_parameters const& args)
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
		return vsm::unexpected(get_last_socket_error());
	}

	return vsm::result<unique_socket_with_flags>(vsm::result_value, socket, handle_flags::none);
}

vsm::result<unique_socket_with_flags> allio::accept_socket(socket_type const listen_socket, socket_address& addr, listen_socket_handle::accept_parameters const& args)
{
	socket_type const socket = ::accept(socket, &addr.addr, &addr.size);

	if (socket == invalid_socket)
	{
		return vsm::unexpected(get_last_socket_error());
	}

	return vsm::result<unique_socket_with_flags>(vsm::result_value, socket, handle_flags::none);
}
