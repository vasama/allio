#include <allio/linux/detail/io_uring/listen_socket.hpp>

#include <allio/impl/linux/socket.hpp>
#include <allio/linux/io_uring_record_context.hpp>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

using M = io_uring_multiplexer;
using H = raw_listen_socket_t::native_type;
using C = async_connector_t<M, raw_listen_socket_t>;

using socket_handle_type = async_handle<raw_socket_t, basic_multiplexer_handle<M>>;
using accept_result_type = accept_result<socket_handle_type>;


using listen_t = raw_listen_socket_t::listen_t;
using listen_s = async_operation_t<M, raw_listen_socket_t, listen_t>;
using listen_a = io_parameters_t<raw_listen_socket_t, listen_t>;

io_result<void> listen_s::submit(M& m, H& h, C& c, listen_s&, listen_a const& a, io_handler<M>&)
{
	vsm_try(addr, posix::socket_address::make(a.endpoint));
	vsm_try(protocol, posix::choose_protocol(addr.addr.sa_family, SOCK_STREAM));

	vsm_try_bind((socket, flags), posix::create_socket(
		addr.addr.sa_family,
		SOCK_STREAM,
		protocol,
		a.flags));

	vsm_try_void(posix::socket_listen(
		socket.get(),
		addr,
		a.backlog));

	vsm_try_void(m.attach_handle(
		posix::wrap_socket(socket.get()),
		c));

	h = H
	{
		platform_object_t::native_type
		{
			object_t::native_type
			{
				raw_listen_socket_t::flags::not_null | flags,
			},
			posix::wrap_socket(socket.release()),
		}
	};

	return {};
}

io_result<void> listen_s::notify(M&, H&, C&, listen_s&, listen_a const&, M::io_status_type)
{
	vsm_unreachable();
}

void listen_s::cancel(M&, H const&, C const&, listen_s&)
{
}


using accept_t = raw_listen_socket_t::accept_t;
using accept_s = async_operation_t<M, raw_listen_socket_t, accept_t>;
using accept_a = io_parameters_t<raw_listen_socket_t, accept_t>;

io_result<accept_result_type> accept_s::submit(M& m, H const& h, C const& c, accept_s& s, accept_a const& a, io_handler<M>& handler)
{
	posix::socket_address_union& addr = new_address(s.addr_storage);
	s.addr_size = sizeof(posix::socket_address_union);

	io_uring_multiplexer::record_context ctx(m);

	auto const [fd, fd_flags] = ctx.get_fd(c, h.platform_handle);

	vsm_try_discard(ctx.push(
	{
		.opcode = IORING_OP_ACCEPT,
		.flags = fd_flags,
		.fd = fd,
		.addr2 = reinterpret_cast<uintptr_t>(&s.addr_size),
		.addr = reinterpret_cast<uintptr_t>(&addr),
		.accept_flags = a.inheritable ? 0 : SOCK_CLOEXEC,
		.user_data = ctx.get_user_data(handler),
	}));

	vsm_try_void(ctx.commit());

	return io_pending(error::async_operation_pending);
}

io_result<accept_result_type> accept_s::notify(M& m, H const&, C const& c, accept_s& s, accept_a const&, M::io_status_type const status)
{
	// This operation uses no io_slots.
	vsm_assert(status.slot == nullptr);

	if (status.result < 0)
	{
		return vsm::unexpected(static_cast<system_error>(-status.result));
	}

	unique_wrapped_socket socket(posix::wrap_socket(status.result));



	socket_handle_type::connector_type socket_c;
	vsm_try_void(m.attach_handle(socket.get(), socket_c));

	return vsm_lazy(accept_result_type
	{
		socket_handle_type(
			adopt_handle,
			m,
			raw_socket_t::native_type
			{
				platform_object_t::native_type
				{
					object_t::native_type
					{
						object_t::flags::not_null,
					},
					socket.release(),
				},
			},
			vsm_move(socket_c)
		),
		get_address(s.addr_storage).get_network_endpoint(),
	});
}

void accept_s::cancel(M& m, H const& h, C const&, S& s)
{
	//TODO: cancel
}
