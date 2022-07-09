#include "posix_socket.hpp"

#include <allio/linux/detail/undef.i>

using namespace allio;

network_address socket_address_union::get_network_address() const
{
	switch (addr.sa_family)
	{
	case AF_INET:
		return ipv4_address(network_byte_order(ipv4.sin_addr.s_addr), ipv4.sin_port);

#if 0
	case AF_INET6:
		return ipv6_address(ipv6.)
#endif
	}

	return {};
}

result<socket_address> socket_address::make(network_address const& address)
{
	result<socket_address> result = result_value;

	switch (auto& addr = *result; address.kind())
	{
	case network_address_kind::local:
		{
			std::string_view const path = address.local().path().string();
			if (path.size() > unix_socket_max_path)
			{
				return allio_ERROR(make_error_code(std::errc::filename_too_long));
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
			ipv4_address const ip = address.ipv4();
			addr.ipv4.sin_family = AF_INET;
			addr.ipv4.sin_port = ip.port();
			addr.ipv4.sin_addr.s_addr = network_byte_order(ip.address());
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

	return result;
}

result<socket_address> socket_address::get(socket_type const socket)
{
	result<socket_address> result = result_value;
	result->size = sizeof(socket_address_union);

	if (getsockname(socket, &result->addr, &result->size))
	{
		return allio_ERROR(get_last_socket_error());
	}

	return result;
}


result<unique_socket> allio::create_socket(network_address_kind const address_kind, flags const handle_flags)
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
		return allio_ERROR(make_error_code(std::errc::not_supported));
	}

	return create_socket(address_family, handle_flags);
}

result<void> allio::listen_socket(socket_type const socket, network_address const& address, listen_parameters const& args)
{
	int const backlog =
		args.backlog != 0
			? std::min(static_cast<int>(args.backlog), 1)
			: SOMAXCONN;

	allio_TRY(addr, socket_address::make(address));

	if (::bind(socket, &addr.addr, addr.size))
	{
		return allio_ERROR(get_last_socket_error());
	}

	if (::listen(socket, backlog))
	{
		return allio_ERROR(get_last_socket_error());
	}

	return {};
}

result<void> detail::common_socket_handle_base::create_sync(network_address_kind const address_kind, socket_parameters const& args)
{
	allio_ASSERT(!*this);
	allio_TRY(socket, create_socket(address_kind, args.handle_flags));
	return consume_socket_handle(*this, { args.handle_flags }, std::move(socket));
}

result<void> detail::socket_handle_base::connect_sync(network_address const& address)
{
	allio_ASSERT(*this);
	allio_TRY(addr, socket_address::make(address));
	if (::connect(unwrap_socket(get_platform_handle()), &addr.addr, addr.size))
	{
		return allio_ERROR(get_last_socket_error());
	}
	return {};
}

result<void> detail::listen_socket_handle_base::listen_sync(network_address const& address, listen_parameters const& args)
{
	allio_ASSERT(*this);
	return listen_socket(unwrap_socket(get_platform_handle()), address, args);
}

result<accept_result> detail::listen_socket_handle_base::accept_sync(socket_parameters const& create_args) const
{
	allio_ASSERT(*this);

	socket_address addr;
	allio_TRY(socket, accept_socket(unwrap_socket(get_platform_handle()), addr, create_args));

	result<accept_result> result = { result_value };
	allio_TRYV(consume_socket_handle(result->socket, { create_args.handle_flags }, std::move(socket)));
	result->address = addr.get_network_address();
	return result;
}
