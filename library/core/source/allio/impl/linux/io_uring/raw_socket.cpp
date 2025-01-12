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

io_result<void> connect_s::submit(
	M& m,
	H& h,
	C& c,
	connect_s& s,
	connect_a const& a,
	io_handler<M>& handler)
{
	posix::socket_address_union& addr = new_address(s.addr_storage);

	vsm_try(addr_size, posix::socket_address::make(a.endpoint, addr));
	vsm_try(protocol, posix::choose_protocol(addr.addr.sa_family, SOCK_STREAM));

	vsm_try_bind((socket, flags), posix::create_socket(
		addr.addr.sa_family,
		SOCK_STREAM,
		protocol,
		a.flags));

	io_uring_multiplexer::record_context ctx(m);

	vsm_try_discard(ctx.push(
	{
		.opcode = IORING_OP_CONNECT,
		.fd = socket.get(),
		.off = addr_size,
		.addr = reinterpret_cast<uintptr_t>(&addr),
		.user_data = ctx.get_user_data(handler),
	}));

	if (a.deadline != deadline::never())
	{
		vsm_try_void(ctx.link_timeout(s.timeout.set(a.deadline)));
	}

	vsm_try_void(ctx.commit());

	s.socket = unique_wrapped_socket(posix::wrap_socket(socket.release()));

	// The POSIX implementation doesn't have any socket flags.
	vsm_assert(flags == handle_flags::none);

	return io_pending(error::operation_pending);
}

io_result<void> connect_s::notify(
	M&,
	H& h,
	C&,
	connect_s& s,
	connect_a const&,
	M::io_status_type const status)
{
	// This operation uses no io_slots.
	vsm_assert(status.slot == nullptr);

	if (status.result < 0)
	{
		return vsm::unexpected(static_cast<system_error>(-status.result));
	}

	h = H
	{
		native_handle<platform_object_t>
		{
			native_handle<object_t>
			{
				object_t::flags::not_null,
			},
			s.socket.release(),
		}
	};

	return {};
}

void connect_s::cancel(M&, H const&, C const&, S& s)
{
}


//TODO: Detect the iovec layout automatically.
static constexpr auto layout = new_io_buffer_layout::data_size;

using read_t = byte_io::stream_read_t;
using read_s = async_operation_t<M, raw_socket_t, read_t>;
using read_a = io_parameters_t<raw_socket_t, read_t>;

io_result<size_t> read_s::submit(
	M& m,
	H const& h,
	C const& c,
	read_s& s,
	read_a const& a,
	io_handler<M>& handler)
{
	vsm_try(buffers, get_io_buffers(s.buffers_storage, a.buffers, layout));

	vsm_try(buffers_size, vsm::try_truncate<uint32_t>(
		buffers.buffers_size,
		error::invalid_argument));

	io_uring_multiplexer::record_context ctx(m);

	auto const [fd, fd_flags] = ctx.get_fd(c, h.platform_handle);

	vsm_try_discard(ctx.push(
	{
		.opcode = IORING_OP_READV,
		.flags = fd_flags,
		.fd = fd,
		.addr = reinterpret_cast<uintptr_t>(buffers.buffers_data),
		.len = buffers_size,
		.user_data = ctx.get_user_data(handler),
	}));

	if (a.deadline != deadline::never())
	{
		vsm_try_void(ctx.link_timeout(s.timeout.set(a.deadline)));
	}

	vsm_try_void(ctx.commit());

	return io_pending(error::operation_pending);
}

io_result<size_t> read_s::notify(
	M&,
	H const& h,
	C const&,
	read_s& s,
	read_a const& a,
	M::io_status_type const status)
{
	// This operation uses no io_slots.
	vsm_assert(status.slot == nullptr);

	if (status.result < 0)
	{
		return vsm::unexpected(static_cast<system_error>(-status.result));
	}

	if (status.result == 0 && !io_buffers_is_empty(a.buffers))
	{
		return vsm::unexpected(error::end_of_stream);
	}

	return static_cast<size_t>(status.result);
}

void read_s::cancel(M&, H const& h, C const&, read_s& s)
{
	//TODO: cancel
}


using write_t = byte_io::stream_write_t;
using write_s = async_operation_t<M, raw_socket_t, write_t>;
using write_a = io_parameters_t<raw_socket_t, write_t>;

io_result<size_t> write_s::submit(
	M& m,
	H const& h,
	C const& c,
	write_s& s,
	write_a const& a,
	io_handler<M>& handler)
{
	vsm_try(buffers, get_io_buffers(s.buffers_storage, a.buffers, layout));

	vsm_try(buffers_size, vsm::try_truncate<uint32_t>(
		buffers.buffers_size,
		error::invalid_argument));

	io_uring_multiplexer::record_context ctx(m);

	auto const [fd, fd_flags] = ctx.get_fd(c, h.platform_handle);

	vsm_try_discard(ctx.push(
	{
		.opcode = IORING_OP_WRITEV,
		.flags = fd_flags,
		.fd = fd,
		.addr = reinterpret_cast<uintptr_t>(buffers.buffers_data),
		.len = buffers_size,
		.user_data = ctx.get_user_data(handler),
	}));

	if (a.deadline != deadline::never())
	{
		vsm_try_void(ctx.link_timeout(s.timeout.set(a.deadline)));
	}

	vsm_try_void(ctx.commit());

	return io_pending(error::operation_pending);
}

io_result<size_t> write_s::notify(
	M&,
	H const& h,
	C const&,
	write_s& s,
	write_a const&,
	M::io_status_type const status)
{
	// This operation uses no io_slots.
	vsm_assert(status.slot == nullptr);

	if (status.result < 0)
	{
		return vsm::unexpected(static_cast<system_error>(-status.result));
	}

	return static_cast<size_t>(status.result);
}

void write_s::cancel(M&, H const& h, C const&, write_s& s)
{
	//TODO: cancel
}
