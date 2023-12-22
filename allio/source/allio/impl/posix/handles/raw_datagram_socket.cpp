#include <allio/detail/handles/raw_datagram_socket.hpp>

#include <allio/impl/posix/socket.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::posix;

vsm::result<void> raw_datagram_socket_t::bind(
	native_handle<raw_datagram_socket_t>& h,
	io_parameters_t<raw_datagram_socket_t, bind_t> const& a)
{
	vsm_try(addr, socket_address::make(a.endpoint));
	vsm_try(protocol, choose_protocol(addr.addr.sa_family, SOCK_DGRAM));

	vsm_try_bind((socket, flags), create_socket(
		addr.addr.sa_family,
		SOCK_DGRAM,
		protocol,
		a.flags));

	vsm_try_void(socket_bind(socket.get(), addr));

	h.flags = flags::not_null | flags;
	h.platform_handle = wrap_socket(socket.release());

	return {};
}

vsm::result<receive_result> raw_datagram_socket_t::receive_from(
	native_handle<raw_datagram_socket_t> const& h,
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

	return vsm::result<receive_result>(
		vsm::result_value,
		transferred,
		addr.get_network_endpoint());
}

vsm::result<void> raw_datagram_socket_t::send_to(
	native_handle<raw_datagram_socket_t> const& h,
	io_parameters_t<raw_datagram_socket_t, send_to_t> const& a)
{
	socket_type const socket = unwrap_socket(h.platform_handle);

	vsm_try(addr, socket_address::make(a.endpoint));

	//TODO: Do timeouts on datagram send make any sense? Probably not...
//	if (a.deadline != deadline::never())
//	{
//		vsm_try_void(socket_poll_or_timeout(socket, socket_poll_w, a.deadline));
//	}

	return socket_send_to(
		socket,
		addr,
		a.buffers.buffers());
}

vsm::result<void> raw_datagram_socket_t::close(
	native_handle<raw_datagram_socket_t>& h,
	io_parameters_t<raw_datagram_socket_t, close_t> const&)
{
	posix::close_socket(unwrap_socket(h.platform_handle));
	h = {};
	return {};
}
