#include <allio/detail/handles/datagram_socket.hpp>

#include <allio/impl/posix/socket.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::posix;

vsm::result<void> raw_datagram_socket_t::bind(
	native_type& h,
	io_parameters_t<raw_datagram_socket_t, bind_t> const& a)
{
	vsm_try(addr, socket_address::make(a.endpoint));
	vsm_try(protocol, choose_protocol(addr.addr.sa_family, SOCK_DGRAM));

	vsm_try_bind((socket, flags), create_socket(
		addr.addr.sa_family,
		SOCK_DGRAM,
		protocol,
		/* multiplexable: */ false));

	vsm_try_void(socket_bind(socket.get(), addr));

	h = native_type
	{
		platform_object_t::native_type
		{
			object_t::native_type
			{
				flags::not_null | flags,
			},
			wrap_socket(socket.release()),
		}
	};

	return {};
}

vsm::result<receive_result> raw_datagram_socket_t::receive_from(
	native_type const& h,
	io_parameters_t<raw_datagram_socket_t, receive_from_t> const& a)
{
	socket_type const socket = unwrap_socket(h.platform_handle);

	if (a.deadline != deadline::never())
	{
		vsm_try_void(socket_poll_or_timeout(socket, socket_poll_r, a.deadline));
	}

	socket_address addr;

	vsm_try(transferred, socket_receive_from(
		socket,
		addr,
		a.buffers.buffers()));

	return vsm_lazy(receive_result
	{
		.size = transferred,
		.endpoint = addr.get_network_endpoint(),
	});
}

vsm::result<void> raw_datagram_socket_t::send_to(
	native_type const& h,
	io_parameters_t<raw_datagram_socket_t, send_to_t> const& a)
{
	socket_type const socket = unwrap_socket(h.platform_handle);

	vsm_try(addr, socket_address::make(a.endpoint));

	if (a.deadline != deadline::never())
	{
		vsm_try_void(socket_poll_or_timeout(socket, socket_poll_w, a.deadline));
	}

	return socket_send_to(
		socket,
		addr,
		a.buffers.buffers());
}

vsm::result<void> raw_datagram_socket_t::close(
	native_type& h,
	io_parameters_t<raw_datagram_socket_t, close_t> const&)
{
	if (h.platform_handle != native_platform_handle::null)
	{
		unrecoverable(posix::close_socket(unwrap_socket(h.platform_handle)));
		h.platform_handle = native_platform_handle::null;
	}

	return {};
}
