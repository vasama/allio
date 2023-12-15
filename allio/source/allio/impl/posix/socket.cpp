#include <allio/impl/posix/socket.hpp>

#include <allio/step_deadline.hpp>

#include <vsm/defer.hpp>
#include <vsm/lazy.hpp>
#include <vsm/utility.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::posix;

template<typename To, typename From>
static To ipv6_network_byte_order(From const& addr)
{
	struct ipv6_data
	{
		uint64_t h;
		uint64_t l;
	};
	auto const data = std::bit_cast<ipv6_data>(addr);
	return std::bit_cast<To>(ipv6_data
	{
		.h = network_byte_order(data.l),
		.l = network_byte_order(data.h),
	});
}

network_endpoint socket_address_union::get_network_endpoint() const
{
	switch (addr.sa_family)
	{
	case AF_UNIX:
		return local_address(path_view());

	case AF_INET:
		return ipv4_endpoint
		{
			.address = ipv4_address(network_byte_order(ipv4.sin_addr.s_addr)),
			.port = network_byte_order(ipv4.sin_port),
		};

	case AF_INET6:
		return ipv6_endpoint
		{
			.address = ipv6_network_byte_order<ipv6_address>(ipv6.sin6_addr),
			.port = network_byte_order(ipv6.sin6_port),
			.zone = network_byte_order(ipv6.sin6_scope_id),
		};
	}

	return {};
}

vsm::result<socket_address_size_type> socket_address::make(
	network_endpoint const& endpoint,
	socket_address_union& addr)
{
	socket_address_size_type addr_size = 0;

	switch (endpoint.kind())
	{
	case network_address_kind::local:
		{
			std::string_view const path = endpoint.local().path().string();
			if (path.size() > unix_socket_max_path)
			{
				return vsm::unexpected(error::filename_too_long);
			}
			addr.unix.sun_family = AF_UNIX;
			memcpy(addr.unix.sun_path, path.data(), path.size());
			if (path.size() < unix_socket_max_path)
			{
				addr.unix.sun_path[path.size()] = '\0';
			}
			addr_size = sizeof(addr.unix);
		}
		break;

	case network_address_kind::ipv4:
		{
			ipv4_endpoint const ip = endpoint.ipv4();
			addr.ipv4.sin_family = AF_INET;
			addr.ipv4.sin_port = network_byte_order(ip.port);
			addr.ipv4.sin_addr.s_addr = network_byte_order(ip.address.integer());
			memset(addr.ipv4.sin_zero, 0, sizeof(ipv4.sin_zero));
			addr_size = sizeof(addr.ipv4);
		}
		break;

	case network_address_kind::ipv6:
		{
			ipv6_endpoint const ip = endpoint.ipv6();
			addr.ipv6.sin6_family = AF_INET6;
			addr.ipv6.sin6_port = network_byte_order(ip.port);
			addr.ipv6.sin6_flowinfo = 0;
			addr.ipv6.sin6_addr = ipv6_network_byte_order<in6_addr>(ip.address);
			addr.ipv6.sin6_scope_id = network_byte_order(ip.zone);
			addr_size = sizeof(addr.ipv6);
		}
		break;

	default:
		return vsm::unexpected(error::unsupported_operation);
	}

	return addr_size;
}

vsm::result<socket_address> socket_address::make(network_endpoint const& endpoint)
{
	vsm::result<socket_address> r(vsm::result_value);
	vsm_try_assign(r->size, make(endpoint, *r));
	return r;
}

vsm::result<socket_address> socket_address::get(socket_type const socket)
{
	vsm::result<socket_address> r(vsm::result_value);
	r->size = sizeof(socket_address_union);

	if (getsockname(socket, &r->addr, &r->size))
	{
		return vsm::unexpected(get_last_socket_error());
	}

	return r;
}


vsm::result<void> posix::socket_listen(
	socket_type const socket,
	socket_address const& addr,
	uint32_t const* const p_backlog)
{
	int const backlog = p_backlog != nullptr
		? std::min<uint32_t>(*p_backlog, std::numeric_limits<int>::max())
		: SOMAXCONN;

	//TODO: Separate bind.
	vsm_try_void(socket_bind(socket, addr));

	if (::listen(socket, backlog) == socket_error_value)
	{
		return vsm::unexpected(get_last_socket_error());
	}

	return {};
}

vsm::result<socket_with_flags> posix::socket_accept(
	socket_type const listen_socket,
	socket_address& addr,
	deadline const deadline)
{
	if (deadline != deadline::never())
	{
		vsm_try_void(socket_poll_or_timeout(listen_socket, socket_poll_r, deadline));
	}

	//TODO: CLOEXEC
	addr.size = sizeof(socket_address_union);
	socket_type const socket = accept(
		listen_socket,
		&addr.addr,
		&addr.size);

	if (socket == socket_error_value)
	{
		return vsm::unexpected(get_last_socket_error());
	}

	return vsm_lazy(socket_with_flags
	{
		.socket = unique_socket(socket),
	});
}


static vsm::result<void> socket_connect_with_timeout(
	socket_type const socket,
	socket_address const& addr,
	deadline const deadline)
{
	vsm_try_void(socket_set_non_blocking(socket, true));

	if (::connect(socket, &addr.addr, addr.size) == socket_error_value)
	{
		switch (socket_error const error = get_last_socket_error())
		{
		case socket_error::would_block:
		case socket_error::in_progress:
			break;

		default:
			return vsm::unexpected(error);
		}
	}

	auto const r = [&]() -> vsm::result<void>
	{
		if (deadline == deadline::instant())
		{
			return vsm::unexpected(error::async_operation_timed_out);
		}

		step_deadline step_deadline(deadline);

		vsm_try(relative_deadline, step_deadline.step());
		vsm_try_void(socket_poll_or_timeout(socket, socket_poll_w, relative_deadline));

		socket_address addr;
		addr.size = sizeof(socket_address_union);

		// Check if the socket is connected by attempting to getting the peer address.
		if (getpeername(socket, &addr.addr, &addr.size) == socket_error_value)
		{
			auto const getpeername_error = get_last_socket_error();

			if (getpeername_error != socket_error::not_connected)
			{
				return vsm::unexpected(error::unknown_failure);
			}

			// If connecting failed, use recv to get the reason for the connect failure.
			char dummy_buffer;
			int const recv_result = recv(socket, &dummy_buffer, 1, 0);
			vsm_assert(recv_result == socket_error_value);

			return recv_result == socket_error_value
				? vsm::unexpected(std::error_code(get_last_socket_error()))
				: vsm::unexpected(std::error_code(error::unknown_failure));
		}

		vsm_try_void(socket_set_non_blocking(socket, false));

		return {};
	}();

	if (!r)
	{
		close_socket(socket);
	}

	return r;
}

vsm::result<void> posix::socket_connect(
	socket_type const socket,
	socket_address const& addr,
	deadline const deadline)
{
	if (deadline != deadline::never())
	{
		return socket_connect_with_timeout(socket, addr, deadline);
	}

	if (::connect(socket, &addr.addr, addr.size) == socket_error_value)
	{
		return vsm::unexpected(get_last_socket_error());
	}

	return {};
}
