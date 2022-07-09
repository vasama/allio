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
#include allio_detail_STR(allio_detail_PLATFORM/posix_socket.hpp)
#undef allio_detail_SOCKET_API

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

template<std::derived_from<detail::common_socket_handle_base> Handle>
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


result<unique_socket> create_socket(int address_family, flags handle_flags);
result<unique_socket> create_socket(network_address_kind address_kind, flags handle_flags);

result<void> listen_socket(socket_type socket, network_address const& address, listen_parameters const& args);
result<unique_socket> accept_socket(socket_type const listen_socket, socket_address& addr, socket_parameters const& create_args);

} // namespace allio

#include <allio/linux/detail/undef.i>
