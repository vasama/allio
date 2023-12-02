#include <allio/detail/handles/listen_socket.hpp>

#include <allio/impl/posix/socket.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::posix;

using accept_result_type = accept_result<blocking_handle<raw_socket_t>>;

vsm::result<void> raw_listen_socket_t::listen(
	native_type& h,
	io_parameters_t<raw_listen_socket_t, listen_t> const& a)
{
	vsm_try(addr, socket_address::make(a.endpoint));
	vsm_try(protocol, choose_protocol(addr.addr.sa_family, SOCK_STREAM));

	vsm_try_bind((socket, flags), create_socket(
		addr.addr.sa_family,
		SOCK_STREAM,
		protocol,
		socket_flags::none));

	vsm_try_void(socket_listen(
		socket.get(),
		addr,
		a.backlog));

	h = native_type
	{
		platform_object_t::native_type
		{
			{
				flags::not_null | flags,
			},
			wrap_socket(socket.release()),
		}
	};

	return {};
}

vsm::result<accept_result_type> raw_listen_socket_t::accept(
	native_type const& h,
	io_parameters_t<raw_listen_socket_t, accept_t> const& a)
{
	socket_address addr;
	vsm_try(socket, socket_accept(
		unwrap_socket(h.platform_handle),
		addr,
		a.deadline));

	return vsm_lazy(accept_result_type
	{
		blocking_handle<raw_socket_t>
		(
			adopt_handle_t(),
			raw_socket_t::native_type
			{
				platform_object_t::native_type
				{
					{
						flags::not_null | socket.flags,
					},
					wrap_socket(socket.socket.release()),
				}
			}
		),
		addr.get_network_endpoint(),
	});
}

vsm::result<void> raw_listen_socket_t::close(
	native_type& h,
	io_parameters_t<raw_listen_socket_t, close_t> const&)
{
	unrecoverable(posix::close_socket(unwrap_socket(h.platform_handle)));
	zero_native_handle(h);
	return {};
}
