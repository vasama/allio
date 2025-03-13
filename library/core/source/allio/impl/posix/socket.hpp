#pragma once

#include <allio/byte_io_buffers.hpp>
#include <allio/error.hpp>
#include <allio/deadline.hpp>
#include <allio/detail/handle_flags.hpp>
#include <allio/detail/object.hpp>
#include <allio/detail/platform.hpp>
#include <allio/impl/storage_provider.hpp>
#include <allio/network.hpp>

#include <vsm/assert.h>
#include <vsm/flags.hpp>
#include <vsm/preprocessor.h>
#include <vsm/result.hpp>
#include <vsm/unique_resource.hpp>

#include <allio/linux/detail/undef.i>

#define allio_detail_socket_api
#include vsm_pp_include(allio/impl/vsm_os/posix_socket.hpp)
#undef allio_detail_socket_api

#include <allio/linux/detail/undef.i>

namespace allio::posix {

struct socket_address_view
{
	sockaddr const* addr;
	socket_address_size_type size;
};

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

struct socket_address_size_wrapper
{
	socket_address_size_type size;
};

template<typename SocketAddress>
struct basic_socket_address : socket_address_size_wrapper, SocketAddress
{
};

struct socket_address : basic_socket_address<socket_address_union>
{
	[[nodiscard]] operator socket_address_view() const
	{
		return { &addr, size };
	}


	[[nodiscard]] vsm::result<socket_address_view> get(any_endpoint_view endpoint);

#if 0
	[[nodiscard]] static vsm::result<socket_address_size_type> set(
		any_endpoint_view endpoint,
		socket_address_union& address_union);

	[[nodiscard]] static vsm::result<socket_address> make(any_endpoint_view endpoint);
#endif

	[[nodiscard]] static vsm::result<socket_address> get(socket_type const socket);
};


using socket_address_storage = inplace_storage_provider<basic_socket_address<socket_address_union>>;

[[nodiscard]] vsm::result<socket_address_view> get_socket_address(
	any_endpoint_view endpoint,
	storage_provider_ref storage_provider);


[[nodiscard]] inline size_t get_max_socket_address_size(int const address_family)
{
	switch (address_family)
	{
	case AF_UNIX:
		return sizeof(sockaddr_un);

	case AF_INET:
		return sizeof(sockaddr_in);

	case AF_INET6:
		return sizeof(sockaddr_in6);
	}

	vsm_assert(false); //PRECONDITION
	return 0;
}

[[nodiscard]] inline size_t get_socket_address_size(sockaddr_un const& addr, size_t const size)
{
	static constexpr size_t path_offset = offsetof(sockaddr_un, sun_path);
	size_t const path_size = strnlen(addr.sun_path, size - path_offset);
	return std::min(size, std::min(sizeof(sockaddr_un), path_offset + path_size + 1));
}

[[nodiscard]] inline size_t get_socket_address_size(sockaddr const& addr, size_t const size)
{
	vsm_assert(size >= offsetof(sockaddr, sa_family) + sizeof(addr.sa_family)); //PRECONDITION

	switch (addr.sa_family)
	{
	case AF_UNIX:
		return get_socket_address_size(reinterpret_cast<sockaddr_un const&>(addr), size);

	case AF_INET:
		vsm_assert(size >= sizeof(sockaddr_in));
		return sizeof(sockaddr_in);

	case AF_INET6:
		vsm_assert(size >= sizeof(sockaddr_in6));
		return sizeof(sockaddr_in6);
	}

	return 0;
}

[[nodiscard]] inline vsm::result<int> get_address_family(network_address_kind const address_kind)
{
	switch (address_kind)
	{
	case network_address_kind::local:
		return AF_UNIX;

	case network_address_kind::ipv4:
		return AF_INET;

	case network_address_kind::ipv6:
		return AF_INET6;

	default:
		return vsm::unexpected(allio_error(error::unsupported_operation));
	}
}

[[nodiscard]] inline network_address_kind get_address_kind(int const address_family)
{
	switch (address_family)
	{
	case AF_UNIX:
		return network_address_kind::local;

	case AF_INET:
		return network_address_kind::ipv4;

	case AF_INET6:
		return network_address_kind::ipv6;

	default:
		vsm_assert(false); //PRECONDITION
	}

	return network_address_kind::null;
}


struct socket_with_flags
{
	unique_socket socket;
	detail::handle_flags flags;
};

[[nodiscard]] inline vsm::result<int> choose_protocol(int const address_family, int const type)
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
	return vsm::unexpected(allio_error(error::invalid_argument));
}


[[nodiscard]] vsm::result<socket_with_flags> create_socket(
	int address_family,
	int type,
	int protocol,
	detail::io_flags flags);

[[nodiscard]] inline vsm::result<void> socket_bind(
	socket_type const socket,
	sockaddr const* const addr,
	socket_address_size_type const size)
{
	if (::bind(socket, addr, size) == socket_error_value)
	{
		return vsm::unexpected(allio_error(get_last_socket_error()));
	}
	return {};
}

[[nodiscard]] inline vsm::result<void> socket_bind(
	socket_type const socket,
	socket_address_view const addr)
{
	return socket_bind(socket, addr.addr, addr.size);
}

[[nodiscard]] vsm::result<void> socket_listen(
	socket_type socket,
	socket_address_view addr,
	uint32_t backlog);

[[nodiscard]] vsm::result<socket_with_flags> socket_accept(
	socket_type listen_socket,
	sockaddr* addr,
	socket_address_size_type* addr_size,
	deadline deadline,
	detail::io_flags flags);

[[nodiscard]] vsm::result<void> socket_connect(
	socket_type socket,
	socket_address_view addr,
	deadline deadline);

[[nodiscard]] vsm::result<socket_poll_mask> socket_poll(
	socket_type socket,
	socket_poll_mask mask,
	deadline deadline);

[[nodiscard]] inline vsm::result<void> socket_poll_or_timeout(
	socket_type const socket,
	socket_poll_mask const mask,
	deadline const deadline)
{
	vsm_try(r, socket_poll(socket, mask, deadline));

	if ((r & mask) == 0)
	{
		return vsm::unexpected(allio_error(error::operation_timed_out));
	}

	return {};
}

[[nodiscard]] vsm::result<void> socket_set_non_blocking(
	socket_type socket,
	bool non_blocking);

[[nodiscard]] vsm::result<size_t> socket_scatter_read(
	socket_type socket,
	detail::new_read_buffers buffers);

[[nodiscard]] vsm::result<size_t> socket_gather_write(
	socket_type socket,
	detail::new_write_buffers buffers);

[[nodiscard]] vsm::result<size_t> socket_receive_from(
	socket_type socket,
	sockaddr* addr,
	socket_address_size_type* addr_size,
	detail::new_read_buffers buffers);

[[nodiscard]] vsm::result<void> socket_send_to(
	socket_type socket,
	socket_address_view addr,
	detail::new_write_buffers buffers);

} // namespace allio::posix
