#include <allio/linux/detail/io_uring/raw_datagram_socket.hpp>

#include <allio/impl/linux/byte_io.hpp>
#include <allio/impl/linux/socket.hpp>
#include <allio/linux/io_uring_record_context.hpp>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

static msghdr& new_msghdr(datagram_header_storage& storage)
{
	static_assert(sizeof(datagram_header_storage) >= sizeof(msghdr));
	static_assert(alignof(datagram_header_storage) >= alignof(msghdr));
	return *new (storage.storage) msghdr;
}


using M = io_uring_multiplexer;
using H = native_handle<raw_datagram_socket_t>;
using C = async_connector_t<M, raw_datagram_socket_t>;

using bind_s = async_operation_t<M, raw_datagram_socket_t, bind_t>;
using bind_a = io_parameters_t<raw_datagram_socket_t, bind_t>;

io_result<void> bind_s::submit(M& m, H& h, C& c, bind_s&, bind_a const& a, io_handler<M>&)
{
	//TODO: In kernel 6.11 and above, use IORING_OP_SOCKET, IORING_OP_BIND.

	vsm_try(addr, posix::socket_address::make(a.endpoint));
	vsm_try(protocol, posix::choose_protocol(addr.addr.sa_family, SOCK_DGRAM));

	vsm_try_bind((socket, flags), posix::create_socket(
		addr.addr.sa_family,
		//TODO: Add raw protocol support
		SOCK_DGRAM,
		protocol,
		a.flags));

	vsm_try_void(socket_bind(socket.get(), addr));
	vsm_try_void(m.attach_fd(socket.get(), c));

	h = H
	{
		native_handle<platform_object_t>
		{
			native_handle<object_t>
			{
				object_t::flags::not_null | flags,
			},
			posix::wrap_socket(socket.release()),
		}
	};

	return {};
}

io_result<void> bind_s::notify(M&, H&, C&, bind_s&, bind_a const&, io_handler<M>&, M::io_status_type)
{
	vsm_unreachable();
}

void bind_s::cancel(M&, H const&, C const&, bind_s&)
{
}


//TODO: Detect the iovec layout automatically.
static constexpr auto layout = new_io_buffer_layout::data_size;


using recv_t = receive_from_t;
using recv_s = async_operation_t<M, raw_datagram_socket_t, recv_t>;
using recv_a = io_parameters_t<raw_datagram_socket_t, recv_t>;

static io_result<size_t> _submit_recv(
	M& m,
	H const& h,
	C const& c,
	recv_s& s,
	recv_a const& a)
{
	auto const buffers = get_io_buffers_unchecked(s.buffers_storage, a.buffers, layout);

	msghdr& header = new_msghdr(s.header_storage) =
	{
		.msg_name = &get_address(s.address_storage).addr,
		.msg_namelen = sizeof(posix::socket_address_union),
		// msghdr::msg_iov seems to be non-const-correct.
		.msg_iov = const_cast<iovec*>(reinterpret_cast<iovec const*>(buffers.buffers_data)),
		.msg_iovlen = buffers.buffers_size,
	};

	io_uring_record_context ctx(m, a.deadline);
	auto const [fd, fd_flags] = ctx.get_fd(c, h.platform_handle);

	vsm_try_ptr(sqe, ctx.push());

	sqe =
	{
		.opcode = IORING_OP_RECVMSG,
		.flags = fd_flags,
		.fd = fd,
		.addr = reinterpret_cast<uintptr_t>(&header),
		.user_data = ctx.get_user_data(s),
	};

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
	(void)new_address(s.address_storage);
	vsm_try_discard(get_io_buffers(s.buffers_storage, a.buffers, layout));

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

	posix::socket_address_union& addr = get_address(s.address_storage);

	return io_result<size_t>(
		vsm::result_value,
		static_cast<size_t>(status.result),
		addr.get_network_endpoint());
}

void recv_s::cancel(M&, H const& h, C const&, recv_s& s)
{
	//TODO: Cancel
}


using send_t = send_to_t;
using send_s = async_operation_t<M, raw_datagram_socket_t, send_t>;
using send_a = io_parameters_t<raw_datagram_socket_t, send_t>;

static io_result<void> _submit_send(
	M& m,
	H const& h,
	C const& c,
	send_s& s,
	send_a const& a)
{
	auto const buffers = get_io_buffers_unchecked(s.buffers_storage, a.buffers, layout);

	msghdr& header = new_msghdr(s.header_storage) =
	{
		.msg_name = &get_address(s.address_storage).addr,
		.msg_namelen = s.addr_size,
		// msghdr::msg_iov seems to be non-const-correct.
		.msg_iov = const_cast<iovec*>(reinterpret_cast<iovec const*>(buffers.buffers_data)),
		.msg_iovlen = buffers.buffers_size,
	};

	io_uring_record_context ctx(m);
	auto const [fd, fd_flags] = ctx.get_fd(c, h.platform_handle);

	vsm_try_ptr(sqe, ctx.push());

	sqe =
	{
		.opcode = IORING_OP_SENDMSG,
		.flags = fd_flags,
		.fd = fd,
		.addr = reinterpret_cast<uintptr_t>(&header),
		.user_data = ctx.get_user_data(s),
	};

	ctx.commit();

	return vsm::unexpected(io_notify_status::submitted);
}

io_result<void> send_s::submit(
	M& m,
	H const& h,
	C const& c,
	send_s& s,
	send_a const& a,
	io_handler<M>& handler)
{
	posix::socket_address_union& addr = new_address(s.address_storage);
	vsm_try_assign(s.addr_size, posix::socket_address::make(a.endpoint, addr));
	vsm_try_discard(get_io_buffers(s.buffers_storage, a.buffers, layout));

	s.set_handler(handler);
	return _submit_send(m, h, c, s, a);
}

io_result<void> send_s::notify(
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

	// The transferred size must match the total specified in the buffers.
	vsm_assert(static_cast<size_t>(status.result) == get_io_buffers_size(a.buffers));

	return {};
}

void send_s::cancel(M& m, H const&, C const&, send_s& s)
{
	(void)m.cancel_io(s);
}
