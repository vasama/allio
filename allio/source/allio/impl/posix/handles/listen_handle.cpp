#include <allio/detail/handles/listen_handle.hpp>

#include <allio/impl/posix/socket.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::posix;

using accept_result_type = accept_result<blocking_handle<raw_socket_handle_t>>;

vsm::result<void> raw_listen_handle_t::blocking_io(
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

vsm::result<void> raw_listen_handle_t::blocking_io(
	listen_t,
	native_type& h,
	io_parameters_t<listen_t> const& args)
{
	vsm_try(addr, socket_address::make(args.endpoint));

	vsm_try(socket, create_socket(
		addr.addr.sa_family,
		/* multiplexable: */ false));

	vsm_try_void(socket_listen(
		socket.socket.get(),
		addr,
		args.backlog));

	h = platform_object_t::native_type
	{
		{
			flags::not_null | socket.flags,
		},
		wrap_socket(socket.socket.release()),
	};

	return {};
}

vsm::result<accept_result_type> raw_listen_handle_t::blocking_io(
	accept_t,
	native_type const& h,
	io_parameters_t<accept_t> const& args)
{
	socket_address addr;
	vsm_try(socket, socket_accept(
		unwrap_socket(h.platform_handle),
		addr,
		args.deadline));

	return vsm_lazy(accept_result_type
	{
		blocking_handle<raw_socket_handle_t>(
			adopt_handle_t(),
			vsm_lazy(platform_object_t::native_type
			{
				{
					flags::not_null | socket.flags,
				},
				wrap_socket(socket.socket.release()),
			})),
		addr.get_network_endpoint(),
	});
}
