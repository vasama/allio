#pragma once

#include <allio/byte_io_buffers.hpp>
#include <allio/detail/platform.h>
#include <allio/network.hpp>

#include <vsm/assert.h>
#include <vsm/preprocessor.h>
#include <vsm/result.hpp>
#include <vsm/unique_resource.hpp>

#include <allio/linux/detail/undef.i>

#define allio_detail_socket_api
#include vsm_pp_include(allio/impl/vsm_os/posix_socket.hpp)
#undef allio_detail_socket_api

#include <allio/linux/detail/undef.i>

namespace allio::posix {

inline vsm::result<int> get_address_family(network_address_kind const address_kind)
{
	switch (address_kind)
	{
	case network_address_kind::local:
		return AF_UNIX;

	case network_address_kind::ipv4:
		return AF_INET;
	}

	return vsm::unexpected(error::unsupported_operation);
}


struct socket_address_union
{
	union
	{
		sockaddr addr;
		sockaddr_un unix;
		sockaddr_in ipv4;
		sockaddr_in6 ipv6;
	};

	[[nodiscard]] network_endpoint get_network_endpoint() const;
};

struct socket_address : socket_address_union
{
	socket_address_size_type size;

	[[nodiscard]] static vsm::result<socket_address> make(network_endpoint const& endpoint);

	[[nodiscard]] static vsm::result<socket_address> get(socket_type const socket);
};


struct socket_deleter
{
	void operator()(socket_type const socket) const
	{
		vsm_verify(close_socket(socket));
	}
};
using unique_socket = vsm::unique_resource<socket_type, socket_deleter, invalid_socket>;


struct unique_socket_with_flags
{
	unique_socket socket;
	handle_flags flags;
};

inline vsm::result<int> choose_protocol(int const address_family, int const type)
{
	switch (address_family)
	{
	case AF_UNIX:
		return 0;

	case AF_INET:
	case AF_INET6:
		switch (type)
		{
		case SOCK_STREAM:
			return IPPROTO_TCP;

		case SOCK_DGRAM:
			return IPPROTO_UDP;
		}
		break;
	}

	//TODO: Use better error code.
	return vsm::unexpected(error::invalid_argument);
}

vsm::result<unique_socket_with_flags> create_socket(
	int address_family,
	int type,
	int protocol,
	bool multiplexable);

inline vsm::result<void> socket_bind(
	socket_type const socket,
	socket_address_union const& addr,
	socket_address_size_type const size)
{
	if (::bind(socket, &addr.addr, size) == socket_error_value)
	{
		return vsm::unexpected(get_last_socket_error());
	}
	return {};
}

inline vsm::result<void> socket_bind(
	socket_type const socket,
	socket_address const& addr)
{
	return socket_bind(socket, addr, addr.size);
}

vsm::result<void> socket_listen(
	socket_type socket,
	socket_address const& addr,
	uint32_t const* backlog);

vsm::result<void> socket_listen(
	socket_type const socket,
	socket_address const& addr,
	std::same_as<std::optional<uint32_t>> auto const& backlog)
{
	return socket_listen(socket, addr, backlog ? &*backlog : nullptr);
}

vsm::result<unique_socket_with_flags> socket_accept(
	socket_type listen_socket,
	socket_address& addr,
	deadline deadline);

vsm::result<void> socket_connect(
	socket_type socket,
	socket_address const& addr,
	deadline deadline);

vsm::result<socket_poll_mask> socket_poll(
	socket_type socket,
	socket_poll_mask mask,
	deadline deadline);

inline vsm::result<void> socket_poll_or_timeout(
	socket_type const socket,
	socket_poll_mask const mask,
	deadline const deadline)
{
	vsm_try(r, socket_poll(socket, mask, deadline));
	
	if ((r & mask) == 0)
	{
		return vsm::unexpected(error::async_operation_timed_out);
	}

	return {};
}

vsm::result<void> socket_set_non_blocking(socket_type socket, bool non_blocking);

vsm::result<size_t> socket_scatter_read(socket_type socket, read_buffers buffers);
vsm::result<size_t> socket_gather_write(socket_type socket, write_buffers buffers);

vsm::result<size_t> socket_receive_from(
	socket_type socket,
	socket_address& address,
	read_buffers buffers);

vsm::result<void> socket_send_to(
	socket_type socket,
	socket_address const& address,
	write_buffers buffers);

} // namespace allio::posix
