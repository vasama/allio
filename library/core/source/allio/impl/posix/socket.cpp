#include <allio/impl/posix/socket.hpp>

#include <allio/impl/error_encoding.hpp>
#include <allio/step_deadline.hpp>

#include <vsm/lazy.hpp>
#include <vsm/numeric.hpp>
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

static vsm::result<socket_address_size_type> set_socket_address(
	sockaddr_un& addr,
	path_view const path)
{
	std::string_view const string = path.string();

	if (string.size() > unix_socket_max_path)
	{
		return vsm::unexpected(allio_error(error::filename_too_long));
	}

	addr.sun_family = AF_UNIX;
	memcpy(addr.sun_path, string.data(), string.size());
	size_t addr_size = offsetof(sockaddr_un, sun_path) + string.size();

	if (string.size() < unix_socket_max_path)
	{
		addr.sun_path[string.size()] = '\0';
		addr_size += 1;
	}

	return static_cast<socket_address_size_type>(addr_size);
}

static vsm::result<socket_address_size_type> set_socket_address(
	sockaddr_in& addr,
	ipv4_endpoint const& endpoint)
{
	addr.sin_family = AF_INET;
	addr.sin_port = network_byte_order(endpoint.port);
	addr.sin_addr.s_addr = network_byte_order(endpoint.address.integer());
	memset(addr.sin_zero, 0, sizeof(addr.sin_zero));
	return static_cast<socket_address_size_type>(sizeof(addr));
}

static vsm::result<socket_address_size_type> set_socket_address(
	sockaddr_in6& addr,
	ipv6_endpoint const& endpoint)
{
	addr.sin6_family = AF_INET6;
	addr.sin6_port = network_byte_order(endpoint.port);
	addr.sin6_flowinfo = 0;
	addr.sin6_addr = ipv6_network_byte_order<in6_addr>(endpoint.address);
	addr.sin6_scope_id = network_byte_order(endpoint.zone);
	return static_cast<socket_address_size_type>(sizeof(addr));
}

#if 0
[[nodiscard]] vsm::result<vsm::allocation> get_socket_address_storage(
	size_t const size,
	any_endpoint_storage_provider const primary_storage_provider,
	any_endpoint_storage_provider const secondary_storage_provider)
{
	vsm_assert(secondary_storage_provider); //PRECONDITION

	if (primary_storage_provider)
	{
		return primary_storage_provider.get_storage(
			size,
			std::align_val_t(alignof(socket_address_union)));
	}

	return secondary_storage_provider.get_storage(
		size,
		std::align_val_t(alignof(socket_address_union)));
}
#endif

#if 0
template<typename SocketAddress>
static vsm::result<sockaddr_buffer> _new_socket_address(
	any_endpoint_storage_provider storage_provider)
{
	vsm_try(storage, storage_provider.get_storage(sizeof(SocketAddress)));
	auto* const addr = ::new (storage) basic_socket_address<SocketAddress>;

	return sockaddr_buffer
	{
		.size = addr->size,
		.addr = reinterpret_cast<sockaddr*>(addr),
	};
}

vsm::result<sockaddr_buffer> new_socket_address(
	network_address_kind const kind,
	any_endpoint_storage_provider storage_provider)
{
	switch (kind)
	{
	case network_address_kind::local:
		return _new_socket_address<sockaddr_un>(storage_provider);

	case network_address_kind::ipv4:
		return _new_socket_address<sockaddr_in>(storage_provider);

	case network_address_kind::ipv6:
		return _new_socket_address<sockaddr_in6>(storage_provider);

	default:
		return vsm::unexpected(allio_error(error::unsupported_operation));
	}
}
#endif


template<typename SocketAddress>
static vsm::result<socket_address_view> get_socket_address_2(
	auto const& endpoint,
	storage_provider_ref const storage_provider)
{
	using socket_address_type = basic_socket_address<SocketAddress>;

	vsm_try(storage, storage_provider.get_storage(
		sizeof(socket_address_type),
		std::align_val_t(alignof(socket_address_type))));

	auto* const addr = ::new (storage) socket_address_type;
	vsm_try_assign(addr->size, set_socket_address(*addr, endpoint));

	return socket_address_view
	{
		.addr = reinterpret_cast<sockaddr*>(static_cast<SocketAddress*>(addr)),
		.size = addr->size,
	};
}

static vsm::result<socket_address_view> get_socket_address_1(
	null_endpoint_t,
	storage_provider_ref const storage_provider)
{
	return vsm::unexpected(allio_error(error::unsupported_operation));
}

static vsm::result<socket_address_view> get_socket_address_1(
	path_view const path,
	storage_provider_ref const storage_provider)
{
	return get_socket_address_2<sockaddr_un>(path, storage_provider);
}

static vsm::result<socket_address_view> get_socket_address_1(
	ipv4_endpoint const& endpoint,
	storage_provider_ref const storage_provider)
{
	return get_socket_address_2<sockaddr_in>(endpoint, storage_provider);
}

static vsm::result<socket_address_view> get_socket_address_1(
	ipv6_endpoint const& endpoint,
	storage_provider_ref const storage_provider)
{
	return get_socket_address_2<sockaddr_in6>(endpoint, storage_provider);
}

static vsm::result<socket_address_view> get_socket_address_1(
	platform_endpoint_view const view,
	storage_provider_ref const storage_provider)
{
	return socket_address_view
	{
		.addr = reinterpret_cast<sockaddr const*>(view.data()),
		.size = vsm::saturating(view.size()),
	};
}

static vsm::result<socket_address_view> get_socket_address_1(
	std::error_code const error,
	storage_provider_ref const storage_provider)
{
	return vsm::unexpected(error);
}

vsm::result<socket_address_view> posix::get_socket_address(
	any_endpoint_view const endpoint,
	storage_provider_ref const storage_provider)
{
	return endpoint.visit([&](auto const& endpoint)
	{
		return get_socket_address_1(endpoint, storage_provider);
	});
}


#if 0
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
#endif

#if 0
vsm::result<socket_address_size_type> socket_address::make(
	any_endpoint_view const endpoint,
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
				return vsm::unexpected(allio_error(error::filename_too_long));
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
		return vsm::unexpected(allio_error(error::unsupported_operation));
	}

	return addr_size;
}

vsm::result<socket_address> socket_address::make(network_endpoint const& endpoint)
{
	vsm::result<socket_address> r(vsm::result_value);
	vsm_try_assign(r->size, make(endpoint, *r));
	return r;
}
#endif

vsm::result<socket_address> socket_address::get(socket_type const socket)
{
	vsm::result<socket_address> r(vsm::result_value);
	r->size = sizeof(socket_address_union);

	if (getsockname(socket, &r->addr, &r->size))
	{
		return vsm::unexpected(allio_error(get_last_socket_error()));
	}

	return r;
}


vsm::result<void> posix::socket_listen(
	socket_type const socket,
	socket_address_view const addr,
	uint32_t const _backlog)
{
	int const backlog = _backlog == 0
		? SOMAXCONN
		: vsm::saturate<int>(_backlog);

	//TODO: Separate bind.
	vsm_try_void(socket_bind(socket, addr));

	if (::listen(socket, backlog) == socket_error_value)
	{
		return vsm::unexpected(allio_error(get_last_socket_error()));
	}

	return {};
}

static vsm::result<void> socket_connect_with_timeout(
	socket_type const socket,
	socket_address_view const addr,
	deadline const deadline)
{
	vsm_try_void(socket_set_non_blocking(socket, true));

	if (::connect(socket, addr.addr, addr.size) == socket_error_value)
	{
		switch (socket_error const error = get_last_socket_error())
		{
		case socket_error::would_block:
		case socket_error::in_progress:
			break;

		default:
			return vsm::unexpected(allio_error(error));
		}
	}

	auto const r = [&]() -> vsm::result<void>
	{
		if (deadline == deadline::instant())
		{
			return vsm::unexpected(allio_error(error::operation_timed_out));
		}

		step_deadline step_deadline(deadline);

		vsm_try(relative_deadline, step_deadline.step());
		vsm_try_void(socket_poll_or_timeout(socket, socket_poll_w, relative_deadline));

		socket_address addr;
		addr.size = sizeof(socket_address_union);

		// Check if the socket is connected by attempting to getting the peer address.
		if (getpeername(socket, &addr.addr, &addr.size) == socket_error_value)
		{
			auto const getpeername_error = allio_error(get_last_socket_error());

			if (getpeername_error != socket_error::not_connected)
			{
				return vsm::unexpected(allio_error(error::unknown_failure));
			}

			// If connecting failed, use recv to get the reason for the connect failure.
			char dummy_buffer[1];
			std::make_signed_t<size_t> const recv_result = recv(
				socket,
				dummy_buffer,
				/* len: */ 1,
				/* flags: */ 0);
			vsm_assert(recv_result == socket_error_value);

			return recv_result == socket_error_value
				? vsm::unexpected(std::error_code(allio_error(get_last_socket_error())))
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
	socket_address_view const addr,
	deadline const deadline)
{
	if (deadline != deadline::never())
	{
		return socket_connect_with_timeout(socket, addr, deadline);
	}

	if (::connect(socket, addr.addr, addr.size) == socket_error_value)
	{
		return vsm::unexpected(allio_error(get_last_socket_error()));
	}

	return {};
}
