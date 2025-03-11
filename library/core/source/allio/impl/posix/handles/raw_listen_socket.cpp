#include <allio/detail/handles/raw_listen_socket.hpp>

#include <allio/impl/posix/socket.hpp>

#include <vsm/lazy.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::posix;

using socket_handle_type = basic_detached_handle<raw_socket_t>;

vsm::result<void> raw_listen_socket_t::listen(
	native_handle<raw_listen_socket_t>& h,
	io_parameters_t<raw_listen_socket_t, listen_t> const& a)
{
	vsm_try(addr, socket_address::make(a.endpoint));
	vsm_try(protocol, choose_protocol(addr.addr.sa_family, SOCK_STREAM));

	vsm_try_bind((socket, flags), create_socket(
		addr.addr.sa_family,
		SOCK_STREAM,
		protocol,
		a.flags));

	vsm_try_void(socket_listen(
		socket.get(),
		addr,
		a.backlog));

	h.flags = flags::not_null | flags;
	h.platform_handle = wrap_socket(socket.release());

	return {};
}

vsm::result<socket_handle_type> raw_listen_socket_t::accept(
	native_handle<raw_listen_socket_t> const& h,
	io_parameters_t<raw_listen_socket_t, accept_t> const& a)
{
	socket_address addr;
	vsm_try_bind((socket, flags), socket_accept(
		unwrap_socket(h.platform_handle),
		addr,
		a.deadline,
		a.flags));

	return vsm_lazy(socket_handle_type(
		adopt_handle,
		native_handle<raw_socket_t>
		{
			native_handle<platform_object_t>
			{
				native_handle<object_t>
				{
					object_t::flags::not_null | flags,
				},
				wrap_handle(socket.release()),
			},
		}));
}

vsm::result<void> raw_listen_socket_t::close(
	native_handle<raw_listen_socket_t>& h,
	io_parameters_t<raw_listen_socket_t, close_t> const&)
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
