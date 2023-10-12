#include <allio/detail/handles/listen_handle.hpp>

#include <allio/impl/posix/socket.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::posix;

vsm::result<void> raw_listen_handle_t::do_blocking_io(
	raw_listen_handle_t& h,
	io_result_ref_t<listen_t>,
	io_parameters_t<listen_t> const& args)
{
	if (h)
	{
		return vsm::unexpected(error::handle_is_not_null);
	}

	vsm_try(addr, socket_address::make(args.endpoint));
	vsm_try(socket, create_socket(addr.addr.sa_family));

	vsm_try_void(socket_listen(
		socket.socket.get(),
		addr,
		args.backlog ? &*args.backlog : nullptr));

	raw_socket_handle_t::native_handle_type native =
	{
		{
			{
				flags::not_null | socket.flags,
			},
			wrap_socket(socket.socket.get()),
		},
	};

	vsm_assert(h.check_native_handle(native));
	h.set_native_handle(vsm_move(native));

	(void)socket.socket.release();

	return {};
}

vsm::result<void> raw_listen_handle_t::do_blocking_io(
	raw_listen_handle_t const& h,
	io_result_ref_t<accept_t> const result,
	io_parameters_t<accept_t> const& args)
{
	if (!h)
	{
		return vsm::unexpected(error::handle_is_null);
	}

	socket_address addr;
	vsm_try(socket, socket_accept(
		unwrap_socket(h.get_platform_handle()),
		addr,
		args.deadline));

	vsm_verify(result.result.socket.set_native_handle(
	{
		{
			{
				handle_flags(flags::not_null) | socket.flags,
			},
			wrap_socket(socket.socket.get()),
		}
	}));
	(void)socket.socket.release();

	result.result.endpoint = addr.get_network_endpoint();

	return {};
}
