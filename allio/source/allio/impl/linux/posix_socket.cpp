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

vsm::result<unique_socket_with_flags> allio::socket_accept(socket_type const socket_listen, socket_address& addr, listen_socket_handle::accept_parameters const& args)
{
	socket_type const socket = ::accept(socket, &addr.addr, &addr.size);

	if (socket == invalid_socket)
	{
		return vsm::unexpected(get_last_socket_error());
	}

	return vsm::result<unique_socket_with_flags>(vsm::result_value, socket, handle_flags::none);
}

vsm::result<void> allio::packet_scatter_read(io::parameters_with_result<io::packet_scatter_read> const& args)
{
	packet_socket_handle const& h = *args.handle;

	if (!h)
	{
		return vsm::unexpected(error::handle_is_null);
	}

	socket_type const socket = unwrap_socket(h.get_platform_handle());

	socket_address addr;

	msghdr message =
	{
		.msg_name = &addr.addr,
		.msg_namelen = sizeof(socket_address_union),
		// msghdr::msg_iov seems to be non-const-correct.
		.msg_iov = (iovec*)args.buffers.data(),
		.msg_iovlen = args.buffers.size(),
	};
	ssize_t const bytes_transferred = recvmsg(socket, &message, 0);

	if (bytes_transferred == -1)
	{
		return vsm::unexpected(get_last_socket_error());
	}

	args.result->packet_size = bytes_transferred;
	args.result->address = addr.get_network_endpoint();

	return {};
}

vsm::result<void> allio::packet_gather_write(io::parameters_with_result<io::packet_gather_write> const& args)
{
	packet_socket_handle const& h = *args.handle;

	if (!h)
	{
		return vsm::unexpected(error::handle_is_null);
	}

	socket_type const socket = unwrap_socket(h.get_platform_handle());

	vsm_try(addr, socket_address::make(*args.address));

	msghdr const message =
	{
		.msg_name = &addr.addr,
		.msg_namelen = addr.size,
		// msghdr::msg_iov seems to be non-const-correct.
		.msg_iov = (iovec*)args.buffers.data(),
		.msg_iovlen = args.buffers.size(),
	};
	ssize_t const bytes_transferred = sendmsg(socket, &message, 0);

	if (bytes_transferred == -1)
	{
		return vsm::unexpected(get_last_socket_error());
	}

	*args.result = bytes_transferred;

	return {};
}
