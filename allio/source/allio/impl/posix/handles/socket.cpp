#include <allio/detail/handles/socket.hpp>

#include <allio/impl/posix/socket.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::posix;

vsm::result<void> raw_socket_t::blocking_io(
	close_t,
	native_type& h,
	io_parameters_t<close_t> const& args)
{
	if (h.platform_handle != native_platform_handle::null)
	{
		unrecoverable(posix::close_socket(unwrap_socket(h.platform_handle)));
		h.platform_handle = native_platform_handle::null;
	}

	return {};
}

vsm::result<void> raw_socket_t::blocking_io(
	connect_t,
	native_type& h,
	io_parameters_t<connect_t> const& args)
{
	vsm_try(addr, socket_address::make(args.endpoint));
	vsm_try(protocol, choose_protocol(addr.addr.sa_family, SOCK_STREAM));

	vsm_try_bind((socket, flags), create_socket(
		addr.addr.sa_family,
		SOCK_STREAM,
		protocol,
		/* multiplexable: */ false));

	vsm_try_void(socket_connect(
		socket.get(),
		addr,
		args.deadline));

	h = platform_object_t::native_type
	{
		object_t::native_type
		{
			flags::not_null | flags,
		},
		wrap_socket(socket.release()),
	};

	return {};
}

vsm::result<void> raw_socket_t::blocking_io(
	disconnect_t,
	native_type& h,
	io_parameters_t<disconnect_t> const& args)
{
	return blocking_io(close_t(), h, {});
}

vsm::result<size_t> raw_socket_t::blocking_io(
	read_some_t,
	native_type const& h,
	io_parameters_t<read_some_t> const& args)
{
	socket_type const socket = unwrap_socket(h.platform_handle);

	if (args.deadline != deadline::never())
	{
		vsm_try_void(socket_poll_or_timeout(socket, socket_poll_r, args.deadline));
	}

	return socket_scatter_read(socket, args.buffers.buffers());
}

vsm::result<size_t> raw_socket_t::blocking_io(
	write_some_t,
	native_type const& h,
	io_parameters_t<write_some_t> const& args)
{
	socket_type const socket = unwrap_socket(h.platform_handle);

	if (args.deadline != deadline::never())
	{
		vsm_try_void(socket_poll_or_timeout(socket, socket_poll_w, args.deadline));
	}

	return socket_gather_write(socket, args.buffers.buffers());
}
