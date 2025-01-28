#include <allio/linux/detail/io_uring/raw_socket.hpp>

#include <allio/impl/linux/socket.hpp>
#include <allio/linux/io_uring_record_context.hpp>

#include <vsm/numeric.hpp>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

using M = io_uring_multiplexer;
using H = native_handle<raw_socket_t>;
using C = async_connector_t<M, raw_socket_t>;

using connect_s = async_operation_t<M, raw_socket_t, connect_t>;
using connect_a = io_parameters_t<raw_socket_t, connect_t>;

static io_result<void> _submit_connect(
	M& m,
	H& h,
	C& c,
	connect_s& s,
	connect_a const& a)
{
	//TODO: In kernel 5.19 and above, use IORING_OP_SOCKET.

	posix::socket_address_union& addr = get_address(s.addr_storage);

	io_uring_record_context ctx(m, a.deadline);

	vsm_try_ptr(sqe, ctx.push());

	sqe =
	{
		.opcode = IORING_OP_CONNECT,
		.fd = s.socket.get(),
		.off = s.addr_size,
		.addr = reinterpret_cast<uintptr_t>(&addr),
		.user_data = ctx.get_user_data(s),
	};

	if (a.deadline != deadline::never())
	{
		vsm_try_void(ctx.link_timeout(s.timeout.set(a.deadline)));
	}

	ctx.commit();

	return vsm::unexpected(io_notify_status::submitted);
}

io_result<void> connect_s::submit(
	M& m,
	H& h,
	C& c,
	connect_s& s,
	connect_a const& a,
	io_handler<M>& handler)
{
	posix::socket_address_union& addr = new_address(s.addr_storage);
	vsm_try_assign(s.addr_size, posix::socket_address::make(a.endpoint, addr));
	vsm_try(protocol, posix::choose_protocol(addr.addr.sa_family, SOCK_STREAM));

	vsm_try_bind((socket, flags), posix::create_socket(
		addr.addr.sa_family,
		SOCK_STREAM,
		protocol,
		a.flags));

	// The POSIX implementation doesn't have any socket flags.
	vsm_assert(flags == handle_flags::none);

	s.socket = vsm_move(socket);

	s.set_handler(handler);
	return _submit_connect(m, h, c, s, a);
}

io_result<void> connect_s::notify(
	M& m,
	H& h,
	C& c,
	connect_s& s,
	connect_a const& a,
	io_handler<M>&,
	M::io_status_type const status)
{
	// This operation uses no io_slots.
	vsm_assert(status.slot == nullptr);

	if (status.result < 0)
	{
		return vsm::unexpected(allio_error(static_cast<system_error>(-status.result)));
	}

	h = H
	{
		native_handle<platform_object_t>
		{
			native_handle<object_t>
			{
				object_t::flags::not_null,
			},
			wrap_handle(s.socket.release()),
		}
	};

	return {};
}

void connect_s::cancel(M& m, H const&, C const&, S& s)
{
	(void)m.cancel_io(s);
}


//TODO: Detect the iovec layout automatically.
static constexpr auto layout = new_io_buffer_layout::data_size;

using recv_t = byte_io::stream_read_t;
using recv_s = async_operation_t<M, raw_socket_t, recv_t>;
using recv_a = io_parameters_t<raw_socket_t, recv_t>;

static io_result<size_t> _submit_recv(
	M& m,
	H const& h,
	C const& c,
	recv_s& s,
	recv_a const& a)
{
	auto const buffers = get_io_buffers_unchecked(s.buffers_storage, a.buffers, layout);

	io_uring_record_context ctx(m, a.deadline);
	auto const [fd, fd_flags] = ctx.get_fd(c, h.platform_handle);

	vsm_try_ptr(sqe, ctx.push());

	sqe =
	{
		.opcode = IORING_OP_READV,
		.flags = fd_flags,
		.fd = fd,
		.addr = reinterpret_cast<uintptr_t>(buffers.buffers_data),
		.len = vsm::truncating(buffers.buffers_size),
		.user_data = ctx.get_user_data(s),
	};

	if (a.deadline != deadline::never())
	{
		vsm_try_void(ctx.link_timeout(s.timeout.set(a.deadline)));
	}

	ctx.commit();

	return vsm::unexpected(io_notify_status::submitted);
}

io_result<size_t> recv_s::submit(
	M& m,
	H const& h,
	C const& c,
	recv_s& s,
	recv_a const& a,
	io_handler<M>& handler)
{
	vsm_try(buffers, get_io_buffers(s.buffers_storage, a.buffers, layout));

	vsm_try_discard(vsm::try_truncate<uint32_t>(
		buffers.buffers_size,
		error::invalid_argument));

	s.set_handler(handler);
	return _submit_recv(m, h, c, s, a);
}

io_result<size_t> recv_s::notify(
	M& m,
	H const& h,
	C const& c,
	recv_s& s,
	recv_a const& a,
	io_handler<M>&,
	M::io_status_type const status)
{
	// This operation uses no io_slots.
	vsm_assert(status.slot == nullptr);

	if (status.result < 0)
	{
		return vsm::unexpected(allio_error(static_cast<system_error>(-status.result)));
	}

	if (status.result == 0 && !io_buffers_is_empty(a.buffers))
	{
		return vsm::unexpected(allio_error(error::end_of_stream));
	}

	return static_cast<size_t>(status.result);
}

void recv_s::cancel(M& m, H const& h, C const&, recv_s& s)
{
	(void)m.cancel_io(s);
}


using send_t = byte_io::stream_write_t;
using send_s = async_operation_t<M, raw_socket_t, send_t>;
using send_a = io_parameters_t<raw_socket_t, send_t>;

static io_result<size_t> _submit_send(
	M& m,
	H const& h,
	C const& c,
	send_s& s,
	send_a const& a)
{
	auto const buffers = get_io_buffers_unchecked(s.buffers_storage, a.buffers, layout);

	io_uring_record_context ctx(m, a.deadline);
	auto const [fd, fd_flags] = ctx.get_fd(c, h.platform_handle);

	vsm_try_ptr(sqe, ctx.push());

	sqe =
	{
		.opcode = IORING_OP_WRITEV,
		.flags = fd_flags,
		.fd = fd,
		.addr = reinterpret_cast<uintptr_t>(buffers.buffers_data),
		.len = vsm::truncating(buffers.buffers_size),
		.user_data = ctx.get_user_data(s),
	};

	if (a.deadline != deadline::never())
	{
		vsm_try_void(ctx.link_timeout(s.timeout.set(a.deadline)));
	}

	ctx.commit();

	return vsm::unexpected(io_notify_status::submitted);
}

io_result<size_t> send_s::submit(
	M& m,
	H const& h,
	C const& c,
	send_s& s,
	send_a const& a,
	io_handler<M>& handler)
{
	vsm_try(buffers, get_io_buffers(s.buffers_storage, a.buffers, layout));

	vsm_try_discard(vsm::try_truncate<uint32_t>(
		buffers.buffers_size,
		error::invalid_argument));

	s.set_handler(handler);
	return _submit_send(m, h, c, s, a);
}

io_result<size_t> send_s::notify(
	M& m,
	H const& h,
	C const& c,
	send_s& s,
	send_a const& a,
	io_handler<M>&,
	M::io_status_type const status)
{
	// This operation uses no io_slots.
	vsm_assert(status.slot == nullptr);

	if (status.result < 0)
	{
		return vsm::unexpected(allio_error(static_cast<system_error>(-status.result)));
	}

	return static_cast<size_t>(status.result);
}

void send_s::cancel(M& m, H const& h, C const&, send_s& s)
{
	(void)m.cancel_io(s);
}
