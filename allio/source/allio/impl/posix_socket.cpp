#include <allio/impl/posix_socket.hpp>

#include <allio/impl/sync_byte_io.hpp>

#include <vsm/utility.hpp>

#include <allio/linux/detail/undef.i>

using namespace allio;

network_address socket_address_union::get_network_address() const
{
	switch (addr.sa_family)
	{
	case AF_INET:
		return ipv4_endpoint{ ipv4_address(network_byte_order(ipv4.sin_addr.s_addr)), ipv4.sin_port };

#if 0
	case AF_INET6:
		return ipv6_address(ipv6.)
#endif
	}

	return {};
}

vsm::result<socket_address> socket_address::make(network_address const& address)
{
	vsm::result<socket_address> r(vsm::result_value);

	switch (auto& addr = *r; address.kind())
	{
	case network_address_kind::local:
		{
			std::string_view const path = address.local().path().string();
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
			addr.size = sizeof(addr.unix);
		}
		break;

	case network_address_kind::ipv4:
		{
			ipv4_endpoint const ip = address.ipv4();
			addr.ipv4.sin_family = AF_INET;
			addr.ipv4.sin_port = ip.port;
			addr.ipv4.sin_addr.s_addr = network_byte_order(ip.address.integer());
			memset(addr.ipv4.sin_zero, 0, sizeof(ipv4.sin_zero));
			addr.size = sizeof(addr.ipv4);
		}
		break;

#if 0
	case network_address_kind::ipv6:
		{
			ipv6_address const ip = address.ipv6();
			addr.ipv6.sin6_family = AF_INET6;
			addr.ipv6.sin6_port = ip.port();
		}
		break;
#endif
	}

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


vsm::result<unique_socket_with_flags> allio::create_socket(network_address_kind const address_kind, common_socket_handle::create_parameters const& args)
{
	int address_family;

	switch (address_kind)
	{
	case network_address_kind::local:
		address_family = AF_UNIX;
		break;

	case network_address_kind::ipv4:
		address_family = AF_INET;
		break;

	default:
		return vsm::unexpected(error::unsupported_operation);
	}

	return create_socket(address_family, args);
}

vsm::result<void> allio::listen_socket(socket_type const socket, network_address const& address, listen_socket_handle::listen_parameters const& args)
{
	int const backlog = std::min<uint32_t>(args.backlog, std::numeric_limits<int>::max());

	vsm_try(addr, socket_address::make(address));

	if (::bind(socket, &addr.addr, addr.size) == socket_error_value)
	{
		return vsm::unexpected(get_last_socket_error());
	}

	if (::listen(socket, backlog) == socket_error_value)
	{
		return vsm::unexpected(get_last_socket_error());
	}

	return {};
}


template<std::derived_from<common_socket_handle> Handle>
static vsm::result<void> sync_create_socket(io::parameters_with_result<io::socket_create> const& args)
{
	//TODO: Add a debug-checked handle cast and use it everywhere.

	Handle& h = static_cast<Handle&>(*args.handle);

	if (h)
	{
		return vsm::unexpected(error::handle_is_not_null);
	}

	vsm_try(socket, create_socket(args.address_kind, args));
	return initialize_socket_handle(h, vsm_move(socket.socket),
		[&](native_platform_handle const socket_handle)
		{
			return platform_handle::native_handle_type
			{
				handle::native_handle_type
				{
					socket.flags | handle::flags::not_null
				},
				socket_handle,
			};
		}
	);
}


vsm::result<void> detail::stream_socket_handle_base::sync_impl(io::parameters_with_result<io::socket_create> const& args)
{
	return sync_create_socket<stream_socket_handle>(args);
}

vsm::result<void> detail::stream_socket_handle_base::sync_impl(io::parameters_with_result<io::socket_connect> const& args)
{
	stream_socket_handle& h = *args.handle;

	if (!h)
	{
		return vsm::unexpected(error::handle_is_null);
	}

	vsm_try(addr, socket_address::make(args.address));
	if (::connect(unwrap_socket(h.get_platform_handle()), &addr.addr, addr.size) == socket_error_value)
	{
		return vsm::unexpected(get_last_socket_error());
	}

	return {};
}

vsm::result<void> detail::stream_socket_handle_base::sync_impl(io::parameters_with_result<io::stream_scatter_read> const& args)
{
	return args.produce(sync_scatter_read(static_cast<platform_handle const&>(*args.handle), args));
}

vsm::result<void> detail::stream_socket_handle_base::sync_impl(io::parameters_with_result<io::stream_gather_write> const& args)
{
	return args.produce(sync_gather_write(static_cast<platform_handle const&>(*args.handle), args));
}

allio_handle_implementation(stream_socket_handle);


vsm::result<void> detail::packet_socket_handle_base::sync_impl(io::parameters_with_result<io::socket_create> const& args)
{
	return sync_create_socket<packet_socket_handle>(args);
}

vsm::result<void> detail::packet_socket_handle_base::sync_impl(io::parameters_with_result<io::packet_scatter_read> const& args)
{
	return packet_scatter_read(args);
}

vsm::result<void> detail::packet_socket_handle_base::sync_impl(io::parameters_with_result<io::packet_gather_write> const& args)
{
	return packet_gather_write(args);
}

allio_handle_implementation(packet_socket_handle);


vsm::result<void> detail::listen_socket_handle_base::sync_impl(io::parameters_with_result<io::socket_create> const& args)
{
	return sync_create_socket<listen_socket_handle>(args);
}

vsm::result<void> detail::listen_socket_handle_base::sync_impl(io::parameters_with_result<io::socket_listen> const& args)
{
	listen_socket_handle& h = *args.handle;

	if (!h)
	{
		return vsm::unexpected(error::handle_is_null);
	}

	return listen_socket(unwrap_socket(h.get_platform_handle()), args.address, args);
}

vsm::result<void> detail::listen_socket_handle_base::sync_impl(io::parameters_with_result<io::socket_accept> const& args)
{
	listen_socket_handle const& h = *args.handle;

	if (!h)
	{
		return vsm::unexpected(error::handle_is_null);
	}

	socket_address addr;
	vsm_try(socket, accept_socket(unwrap_socket(h.get_platform_handle()), addr, args));

	vsm::result<accept_result> result(vsm::result_value);
	vsm_try_void(initialize_socket_handle(
		args.result->socket, vsm_move(socket.socket),
		[&](native_platform_handle const socket_handle)
		{
			return platform_handle::native_handle_type
			{
				handle::native_handle_type
				{
					socket.flags | handle::flags::not_null
				},
				socket_handle,
			};
		}
	));

	args.result->address = addr.get_network_address();

	return {};
}

allio_handle_implementation(listen_socket_handle);
