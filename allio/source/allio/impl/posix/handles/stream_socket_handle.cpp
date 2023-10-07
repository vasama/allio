#include <allio/detail/handles/stream_socket_handle.hpp>

#include <allio/impl/posix/socket.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::posix;

vsm::result<void> _stream_socket_handle::do_blocking_io(
	_stream_socket_handle& h,
	io_result_ref_t<connect_t>,
	io_parameters_t<connect_t> const& args)
{
	if (h)
	{
		return vsm::unexpected(error::handle_is_not_null);
	}

	vsm_try(addr, socket_address::make(args.endpoint));
	vsm_try(socket, create_socket(addr.addr.sa_family));

	vsm_try_void(socket_connect(
		socket.socket.get(),
		addr,
		args.deadline));

	platform_handle::native_handle_type const native =
	{
		{
			flags::not_null | socket.flags,
		},
		wrap_socket(socket.socket.get()),
	};

	vsm_assert(h.check_native_handle(native));
	h.set_native_handle(native);

	(void)socket.socket.release();

	return {};
}

vsm::result<void> _stream_socket_handle::do_blocking_io(
	_stream_socket_handle const& h,
	io_result_ref_t<scatter_read_t> const result,
	io_parameters_t<scatter_read_t> const& args)
{
	if (!h)
	{
		return vsm::unexpected(error::handle_is_not_null);
	}

	socket_type const socket = unwrap_socket(h.get_platform_handle());

	if (args.deadline != deadline::never())
	{
		vsm_try_void(socket_poll_or_timeout(socket, socket_poll_r, args.deadline));
	}

	return result.produce(socket_scatter_read(socket, args.buffers.buffers()));
}

vsm::result<void> _stream_socket_handle::do_blocking_io(
	_stream_socket_handle const& h,
	io_result_ref_t<gather_write_t> const result,
	io_parameters_t<gather_write_t> const& args)
{
	if (!h)
	{
		return vsm::unexpected(error::handle_is_not_null);
	}

	socket_type const socket = unwrap_socket(h.get_platform_handle());

	if (args.deadline != deadline::never())
	{
		vsm_try_void(socket_poll_or_timeout(socket, socket_poll_w, args.deadline));
	}

	return result.produce(socket_gather_write(socket, args.buffers.buffers()));
}
