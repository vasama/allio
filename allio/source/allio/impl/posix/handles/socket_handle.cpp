#include <allio/detail/handles/socket_handle.hpp>

#include <allio/impl/posix/socket.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::posix;

vsm::result<void> raw_socket_handle_t::blocking_io(
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

vsm::result<void> raw_socket_handle_t::blocking_io(
	socket_io::connect_t,
	native_type& h,
	io_parameters_t<connect_t> const& args)
{
	vsm_try(addr, socket_address::make(args.endpoint));
	vsm_try(socket, create_socket(addr.addr.sa_family));

	vsm_try_void(socket_connect(
		socket.socket.get(),
		addr,
		args.deadline));

	h = native_type
	{
		platform_handle_t::native_type
		{
			handle_t::native_type
			{
				flags::not_null | socket.flags,
			},
			wrap_socket(socket.socket.release()),
		},
	};

	return {};
}

vsm::result<void> raw_socket_handle_t::blocking_io(
	socket_io::disconnect_t,
	native_type& h,
	io_parameters_t<disconnect_t> const& args)
{
	return blocking_io(close_t(), h, {});
}

vsm::result<size_t> raw_socket_handle_t::blocking_io(
	socket_io::stream_read_t,
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

vsm::result<size_t> raw_socket_handle_t::blocking_io(
	socket_io::stream_write_t,
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
