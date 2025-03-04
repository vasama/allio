#include <allio/detail/handles/raw_datagram_socket.hpp>

#include <allio/impl/posix/handles/raw_common_socket.hpp>
#include <allio/impl/posix/socket.hpp>

#include <vsm/numeric.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::posix;

vsm::result<void> raw_datagram_socket_t::bind(
	native_handle<raw_datagram_socket_t>& h,
	io_parameters_t<raw_datagram_socket_t, bind_t> const& a)
{
	posix::socket_address_storage address_storage;
	vsm_try(addr, posix::get_socket_address(a.endpoint, address_storage));
	vsm_try(protocol, choose_protocol(addr.addr->sa_family, SOCK_DGRAM));

	vsm_try_bind((socket, flags), create_socket(
		addr.addr->sa_family,
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

	int const address_family = posix::get_address_family(h.flags);
	size_t const max_address_size = posix::get_max_socket_address_size(address_family);

	socket_address_union addr_union;
	socket_address_size_type addr_size = sizeof(addr_union);
	sockaddr* addr = &addr_union.addr;

	if (a.endpoint_storage)
	{
		vsm_try(addr_storage, a.endpoint_storage.get_storage(
			max_address_size,
			std::align_val_t(alignof(posix::socket_address_union))));

		addr = reinterpret_cast<sockaddr*>(addr_storage.storage);
		addr_size = vsm::truncating(addr_storage.size);
	}

	if (a.deadline != deadline::never())
	{
		vsm_try_void(socket_poll_or_timeout(socket, socket_poll_r, a.deadline));
	}

	vsm_try(transferred, socket_receive_from(
		socket,
		sockaddr_buffer(addr_size, addr),
		a.buffers));

	return vsm::result<receive_result>(
		vsm::result_value,
		transferred,
		platform_endpoint_view(addr, addr_size));
}

vsm::result<void> raw_datagram_socket_t::send_to(
	native_handle<raw_datagram_socket_t> const& h,
	io_parameters_t<raw_datagram_socket_t, send_to_t> const& a)
{
	socket_type const socket = unwrap_socket(h.platform_handle);

	posix::socket_address_storage address_storage;
	vsm_try(addr, posix::get_socket_address(a.endpoint, address_storage));

	//TODO: Do timeouts on datagram send make any sense? Probably not...
//	if (a.deadline != deadline::never())
//	{
//		vsm_try_void(socket_poll_or_timeout(socket, socket_poll_w, a.deadline));
//	}

	return socket_send_to(
		socket,
		addr,
		a.buffers);
}

vsm::result<void> raw_datagram_socket_t::close(
	native_handle<raw_datagram_socket_t>& h,
	io_parameters_t<raw_datagram_socket_t, close_t> const& a)
{
	native_platform_handle const handle = h.platform_handle;
	if (handle != native_platform_handle::null)
	{
		h.platform_handle = native_platform_handle::null;
		posix::close_socket(posix::unwrap_socket(handle));
	}

	h.flags = handle_flags::none;

	return {};
}
