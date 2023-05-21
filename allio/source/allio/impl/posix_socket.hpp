#pragma once

#include <allio/socket_handle.hpp>
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

	[[nodiscard]] static vsm::result<socket_address> make(network_address const& address);

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

template<std::derived_from<common_socket_handle> Handle, typename Create>
vsm::result<void> initialize_socket_handle(Handle& managed_handle, unique_socket&& socket, Create&& create)
{
	return vsm::consume_resources([&](socket_type const h) -> vsm::result<void>
	{
		return managed_handle.set_native_handle(static_cast<Create&&>(create)(wrap_socket(h)));
	}, static_cast<unique_socket&&>(socket));
}

template<std::derived_from<common_socket_handle> Handle>
vsm::result<void> consume_socket_handle(Handle& managed_handle, handle::native_handle_type const handle_handle, unique_socket&& socket, auto&&... args)
{
	return vsm::consume_resources([&](socket_type const socket) -> vsm::result<void>
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
	}, static_cast<unique_socket&&>(socket));
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

vsm::result<unique_socket_with_flags> create_socket(int address_family, common_socket_handle::create_parameters const& args);
vsm::result<unique_socket_with_flags> create_socket(network_address_kind address_kind, common_socket_handle::create_parameters const& args);

vsm::result<void> listen_socket(socket_type socket, network_address const& address, listen_socket_handle::listen_parameters const& args);
vsm::result<unique_socket_with_flags> accept_socket(socket_type const listen_socket, socket_address& addr, listen_socket_handle::accept_parameters const& args);

//vsm::result<size_t> socket_scatter_read(socket_type socket, io::scatter_gather_parameters const& args);
//vsm::result<size_t> socket_gather_write(socket_type socket, io::scatter_gather_parameters const& args);

vsm::result<void> packet_scatter_read(io::parameters_with_result<io::packet_scatter_read> const& args);
vsm::result<void> packet_gather_write(io::parameters_with_result<io::packet_gather_write> const& args);

} // namespace allio
