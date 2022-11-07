#pragma once

#include <allio/socket_handle.hpp>
#include <allio/detail/assert.hpp>
#include <allio/detail/platform.hpp>
#include <allio/detail/preprocessor.h>
#include <allio/detail/unique_resource.hpp>
#include <allio/network.hpp>
#include <allio/result.hpp>

#include <allio/linux/detail/undef.i>

#define allio_detail_SOCKET_API
#include allio_detail_STR(allio/impl/allio_detail_PLATFORM/posix_socket.hpp)
#undef allio_detail_SOCKET_API

#include <allio/linux/detail/undef.i>

namespace allio {

struct socket_address_union
{
	union
	{
		sockaddr addr;
		sockaddr_un unix;
		sockaddr_in ipv4;
		sockaddr_in6 ipv6;
	};

	[[nodiscard]] network_address get_network_address() const;
};

struct socket_address : socket_address_union
{
	socket_address_size_type size;

	[[nodiscard]] static result<socket_address> make(network_address const& address);

	[[nodiscard]] static result<socket_address> get(socket_type const socket);
};


struct socket_deleter
{
	void operator()(socket_type const socket) const
	{
		allio_VERIFY(close_socket(socket));
	}
};
using unique_socket = detail::unique_resource<socket_type, socket_deleter, invalid_socket>;

template<std::derived_from<common_socket_handle> Handle, typename Create>
result<void> initialize_socket_handle(Handle& managed_handle, unique_socket&& socket, Create&& create)
{
	return consume_resource(static_cast<unique_socket&&>(socket), [&](socket_type const h) -> result<void>
	{
		return managed_handle.set_native_handle(static_cast<Create&&>(create)(wrap_socket(h)));
	});
}

template<std::derived_from<common_socket_handle> Handle>
result<void> consume_socket_handle(Handle& managed_handle, handle::native_handle_type const handle_handle, unique_socket&& socket, auto&&... args)
{
	return consume_resource(static_cast<unique_socket&&>(socket), [&](socket_type const socket) -> result<void>
	{
		return managed_handle.set_native_handle(
		{
			platform_handle::native_handle_type
			{
				handle_handle,
				wrap_socket(socket),
			},
			static_cast<decltype(args)&&>(args)...
		});
	});
}


struct unique_socket_with_flags
{
	unique_socket socket;
	handle_flags flags;

	unique_socket_with_flags() = default;

	unique_socket_with_flags(socket_type const socket, handle_flags const flags)
		: socket(socket)
		, flags(flags)
	{
	}
};

result<unique_socket_with_flags> create_socket(int address_family, common_socket_handle::create_parameters const& args);
result<unique_socket_with_flags> create_socket(network_address_kind address_kind, common_socket_handle::create_parameters const& args);

result<void> listen_socket(socket_type socket, network_address const& address, listen_socket_handle::listen_parameters const& args);
result<unique_socket_with_flags> accept_socket(socket_type const listen_socket, socket_address& addr, listen_socket_handle::accept_parameters const& args);

//result<size_t> socket_scatter_read(socket_type socket, io::scatter_gather_parameters const& args);
//result<size_t> socket_gather_write(socket_type socket, io::scatter_gather_parameters const& args);

result<void> packet_scatter_read(io::parameters_with_result<io::packet_scatter_read> const& args);
result<void> packet_gather_write(io::parameters_with_result<io::packet_gather_write> const& args);

} // namespace allio
