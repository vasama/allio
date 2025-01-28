#include <allio/linux/detail/io_uring/raw_listen_socket.hpp>

#include <allio/impl/linux/socket.hpp>
#include <allio/linux/io_uring_record_context.hpp>

#include <vsm/lazy.hpp>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

using M = io_uring_multiplexer;
using H = native_handle<raw_listen_socket_t>;
using C = async_connector_t<M, raw_listen_socket_t>;

using socket_handle_type = basic_attached_handle<raw_socket_t, basic_multiplexer_handle<M>>;
using accept_result_type = accept_result<socket_handle_type>;


using listen_s = async_operation_t<M, raw_listen_socket_t, listen_t>;
using listen_a = io_parameters_t<raw_listen_socket_t, listen_t>;

io_result<void> listen_s::submit(M& m, H& h, C& c, listen_s&, listen_a const& a, io_handler<M>&)
{
	//TODO: In kernel 6.11 and above, use IORING_OP_SOCKET, IORING_OP_BIND, IORING_OP_LISTEN.

	vsm_try(addr, posix::socket_address::make(a.endpoint));
	vsm_try(protocol, posix::choose_protocol(addr.addr.sa_family, SOCK_STREAM));

	vsm_try_bind((socket, flags), posix::create_socket(
		addr.addr.sa_family,
		SOCK_STREAM,
		protocol,
		a.flags));

	// The POSIX implementation doesn't have any socket flags.
	vsm_assert(flags == handle_flags::none);

	vsm_try_void(posix::socket_listen(
		socket.get(),
		addr,
		a.backlog));

	vsm_try_void(m.attach_fd(socket.get(), c));

	h = H
	{
		native_handle<platform_object_t>
		{
			native_handle<object_t>
			{
				raw_listen_socket_t::flags::not_null | flags,
			},
			posix::wrap_socket(socket.release()),
		}
	};

	return {};
}

io_result<void> listen_s::notify(M&, H&, C&, listen_s&, listen_a const&, io_handler<M>&, M::io_status_type)
{
	vsm_unreachable();
}

void listen_s::cancel(M&, H const&, C const&, listen_s&)
{
}


using accept_s = async_operation_t<M, raw_listen_socket_t, accept_t>;
using accept_a = io_parameters_t<raw_listen_socket_t, accept_t>;

static io_result<accept_result_type> _submit_accept(
	M& m,
	H const& h,
	C const& c,
	accept_s& s,
	accept_a const& a)
{
	posix::socket_address_union& addr = get_address(s.addr_storage);

	io_uring_record_context ctx(m, a.deadline);
	auto const [fd, fd_flags] = ctx.get_fd(c, h.platform_handle);

	vsm_try_ptr(sqe, ctx.push());

	sqe =
	{
		.opcode = IORING_OP_ACCEPT,
		.flags = fd_flags,
		.fd = fd,
		.addr2 = reinterpret_cast<uintptr_t>(&s.addr_size),
		.addr = reinterpret_cast<uintptr_t>(&addr),
		.accept_flags = vsm::any_flags(a.flags, io_flags::create_inheritable)
			? static_cast<uint32_t>(0)
			: static_cast<uint32_t>(SOCK_CLOEXEC),
		.user_data = ctx.get_user_data(s),
	};

	ctx.commit();

	return vsm::unexpected(io_notify_status::submitted);
}

io_result<accept_result_type> accept_s::submit(
	M& m,
	H const& h,
	C const& c,
	accept_s& s,
	accept_a const& a,
	io_handler<M>& handler)
{
	(void)new_address(s.addr_storage);
	s.addr_size = sizeof(posix::socket_address_union);

	s.set_handler(handler);
	return _submit_accept(m, h, c, s, a);
}

io_result<accept_result_type> accept_s::notify(
	M& m,
	H const& h,
	C const& c,
	accept_s& s,
	accept_a const& a,
	io_handler<M>&,
	M::io_status_type const status)
{
	// This operation uses no io_slots.
	vsm_assert(status.slot == nullptr);

	if (status.result < 0)
	{
		return vsm::unexpected(allio_error(static_cast<system_error>(-status.result)));
	}

	unique_handle socket(status.result);

	socket_handle_type::connector_type socket_c;
	vsm_try_void(m.attach_fd(socket.get(), socket_c));

	return vsm_lazy(accept_result_type
	{
		socket_handle_type(
			adopt_handle,
			m,
			native_handle<raw_socket_t>
			{
				native_handle<platform_object_t>
				{
					native_handle<object_t>
					{
						object_t::flags::not_null,
					},
					wrap_handle(socket.release()),
				},
			},
			vsm_move(socket_c)
		),
		get_address(s.addr_storage).get_network_endpoint(),
	});
}

void accept_s::cancel(M& m, H const& h, C const&, S& s)
{
	(void)m.cancel_io(s);
}
