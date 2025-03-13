#include <allio/linux/detail/io_uring/raw_socket.hpp>

#include <allio/impl/linux/byte_io.hpp>
#include <allio/impl/linux/error.hpp>
#include <allio/impl/posix/handles/raw_common_socket.hpp>
#include <allio/impl/posix/socket.hpp>
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

io_result<void> connect_s::submit(
	M& m,
	H& h,
	C& c,
	connect_s& s,
	connect_a const& a,
	io_handler<M>& handler)
{
	//TODO: In kernel 5.19 and above, use IORING_OP_SOCKET.

	io_extension_allocator extension = initialize_extension(s);

	vsm_try(addr, posix::get_socket_address(a.endpoint, extension));
	vsm_try(protocol, posix::choose_protocol(addr.addr->sa_family, SOCK_STREAM));

	vsm_try_bind((socket, flags), posix::create_socket(
		addr.addr->sa_family,
		SOCK_STREAM,
		protocol,
		a.flags));

	// The POSIX implementation doesn't have any socket flags.
	vsm_assert(flags == handle_flags::none);

	s.socket = vsm_move(socket);
	s.socket_flags = posix::set_address_family(addr.addr->sa_family);

	io_uring_record_context ctx(m, a.deadline);

	vsm_try_ptr(sqe, ctx.push());

	sqe =
	{
		.opcode = IORING_OP_CONNECT,
		.fd = s.socket.get(),
		.off = addr.size,
		.addr = reinterpret_cast<uintptr_t>(addr.addr),
		.user_data = ctx.get_user_data(s),
	};

	if (a.deadline != deadline::never())
	{
		vsm_try_void(ctx.link_timeout(s.timeout.set(a.deadline)));
	}

	s.set_handler(handler);
	ctx.commit();
	extension.release();

	return vsm::unexpected(io_notify_status::submitted);
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
	io_extension_allocator const extension = acquire_extension(s);

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
				object_t::flags::not_null | s.socket_flags,
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


using recv_t = byte_io::stream_read_t;
using recv_s = async_operation_t<M, raw_socket_t, recv_t>;
using recv_a = io_parameters_t<raw_socket_t, recv_t>;

io_result<size_t> recv_s::submit(
	M& m,
	H const& h,
	C const& c,
	recv_s& s,
	recv_a const& a,
	io_handler<M>& handler)
{
	vsm_try_void(check_io_vectors_size(a.buffers));

	io_extension_allocator extension = initialize_extension(s);
	vsm_try(io_vectors, get_io_vectors(a.buffers, extension));

	io_uring_record_context ctx(m, a.deadline);
	auto const [fd, fd_flags] = ctx.get_fd(c, h.platform_handle);

	vsm_try_ptr(sqe, ctx.push());

	sqe =
	{
		.opcode = IORING_OP_READV,
		.flags = fd_flags,
		.fd = fd,
		.addr = reinterpret_cast<uintptr_t>(io_vectors.data()),
		.len = vsm::truncating(io_vectors.size()),
		.user_data = ctx.get_user_data(s),
	};

	if (a.deadline != deadline::never())
	{
		vsm_try_void(ctx.link_timeout(s.timeout.set(a.deadline)));
	}

	s.set_handler(handler);
	ctx.commit();
	extension.release();

	return vsm::unexpected(io_notify_status::submitted);
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
	io_extension_allocator const extension = acquire_extension(s);

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

io_result<size_t> send_s::submit(
	M& m,
	H const& h,
	C const& c,
	send_s& s,
	send_a const& a,
	io_handler<M>& handler)
{
	vsm_try_void(check_io_vectors_size(a.buffers));

	io_extension_allocator extension = initialize_extension(s);
	vsm_try(io_vectors, get_io_vectors(a.buffers, extension));

	io_uring_record_context ctx(m, a.deadline);
	auto const [fd, fd_flags] = ctx.get_fd(c, h.platform_handle);

	vsm_try_ptr(sqe, ctx.push());

	sqe =
	{
		.opcode = IORING_OP_WRITEV,
		.flags = fd_flags,
		.fd = fd,
		.addr = reinterpret_cast<uintptr_t>(io_vectors.data()),
		.len = vsm::truncating(io_vectors.size()),
		.user_data = ctx.get_user_data(s),
	};

	if (a.deadline != deadline::never())
	{
		vsm_try_void(ctx.link_timeout(s.timeout.set(a.deadline)));
	}

	s.set_handler(handler);
	ctx.commit();
	extension.release();

	return vsm::unexpected(io_notify_status::submitted);
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
	io_extension_allocator const extension = acquire_extension(s);

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
