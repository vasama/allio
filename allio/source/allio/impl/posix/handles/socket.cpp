#include <allio/detail/handles/socket.hpp>

#include <allio/impl/posix/socket.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::posix;

vsm::result<void> raw_socket_t::connect(
	native_type& h,
	io_parameters_t<raw_socket_t, connect_t> const& a)
{
	vsm_try(addr, socket_address::make(a.endpoint));
	vsm_try(protocol, choose_protocol(addr.addr.sa_family, SOCK_STREAM));

	vsm_try_bind((socket, flags), create_socket(
		addr.addr.sa_family,
		SOCK_STREAM,
		protocol,
		/* multiplexable: */ false));

	vsm_try_void(socket_connect(
		socket.get(),
		addr,
		a.deadline));

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

vsm::result<size_t> raw_socket_t::stream_read(
	native_type const& h,
	io_parameters_t<raw_socket_t, read_some_t> const& a)
{
	socket_type const socket = unwrap_socket(h.platform_handle);

	if (a.deadline != deadline::never())
	{
		vsm_try_void(socket_poll_or_timeout(socket, socket_poll_r, a.deadline));
	}

	return socket_scatter_read(socket, a.buffers.buffers());
}

vsm::result<size_t> raw_socket_t::stream_write(
	native_type const& h,
	io_parameters_t<raw_socket_t, write_some_t> const& a)
{
	socket_type const socket = unwrap_socket(h.platform_handle);

	if (a.deadline != deadline::never())
	{
		vsm_try_void(socket_poll_or_timeout(socket, socket_poll_w, a.deadline));
	}

	return socket_gather_write(socket, a.buffers.buffers());
}

vsm::result<void> raw_socket_t::close(
	native_type& h,
	io_parameters_t<raw_socket_t, close_t> const&)
{
	if (h.platform_handle != native_platform_handle::null)
	{
		unrecoverable(posix::close_socket(unwrap_socket(h.platform_handle)));
		h.platform_handle = native_platform_handle::null;
	}

	return {};
}
