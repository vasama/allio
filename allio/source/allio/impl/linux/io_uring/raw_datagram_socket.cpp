#include <allio/linux/detail/io_uring/datagram_socket.hpp>

#include <allio/impl/byte_io.hpp>
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

static msghdr& get_msghdr(datagram_header_storage& storage)
{
	return *std::launder(reinterpret_cast<msghdr*>(storage.storage));
}


using M = io_uring_multiplexer;
using H = raw_datagram_socket_t::native_type;
using C = async_connector_t<M, raw_datagram_socket_t>;

using bind_t = raw_datagram_socket_t::bind_t;
using bind_s = async_operation_t<M, raw_datagram_socket_t, bind_t>;
using bind_a = io_parameters_t<raw_datagram_socket_t, bind_t>;

io_result<void> bind_s::submit(M& m, H& h, C& c, bind_s&, bind_a const& a, io_handler<M>&)
{
	vsm_try(addr, posix::socket_address::make(a.endpoint));
	vsm_try(protocol, posix::choose_protocol(addr.addr.sa_family, SOCK_DGRAM));

	vsm_try_bind((socket, flags), posix::create_socket(
		addr.addr.sa_family,
		//TODO: Add raw protocol support
		SOCK_DGRAM,
		protocol,
		a.flags));

	vsm_try_void(socket_bind(socket.get(), addr));

	vsm_try_void(m.attach_handle(
		posix::wrap_socket(socket.get()),
		c));

	h = H
	{
		platform_object_t::native_type
		{
			object_t::native_type
			{
				object_t::flags::not_null | flags,
			},
			posix::wrap_socket(socket.release()),
		}
	};

	return {};
}

io_result<void> bind_s::notify(M&, H&, C&, bind_s&, bind_a const&, M::io_status_type)
{
	vsm_unreachable();
}

void bind_s::cancel(M&, H const&, C const&, bind_s&)
{
}


using send_t = raw_datagram_socket_t::send_to_t;
using send_s = async_operation_t<M, raw_datagram_socket_t, send_t>;
using send_a = io_parameters_t<raw_datagram_socket_t, send_t>;

io_result<void> send_s::submit(M& m, H const& h, C const& c, send_s& s, send_a const& a, io_handler<M>& handler)
{
	posix::socket_address_union& addr = new_address(s.address_storage);
	vsm_try(addr_size, posix::socket_address::make(a.endpoint, addr));

	auto const buffers = a.buffers.buffers();
	msghdr& header = new_msghdr(s.header_storage) =
	{
		.msg_name = &addr.addr,
		.msg_namelen = addr_size,
		.msg_iov = reinterpret_cast<iovec*>(const_cast<write_buffer*>(buffers.data())),
		.msg_iovlen = buffers.size(),
	};

	io_uring_multiplexer::record_context ctx(m);
	auto const [fd, fd_flags] = ctx.get_fd(c, h.platform_handle);

	vsm_try_discard(ctx.push(
	{
		.opcode = IORING_OP_SENDMSG,
		.flags = fd_flags,
		.fd = fd,
		.addr = reinterpret_cast<uintptr_t>(&header),
		.user_data = ctx.get_user_data(handler),
	}));

	vsm_try_void(ctx.commit());

	return io_pending(error::async_operation_pending);
}

io_result<void> send_s::notify(M&, H const& h, C const&, send_s& s, send_a const& a, M::io_status_type const status)
{
	// This operation uses no io_slots.
	vsm_assert(status.slot == nullptr);

	if (status.result < 0)
	{
		return vsm::unexpected(static_cast<system_error>(-status.result));
	}
	vsm_assert(status.result == get_buffers_size(a.buffers.buffers()));

	return {};
}

void send_s::cancel(M&, H const& h, C const&, send_s& s)
{
}


using recv_t = raw_datagram_socket_t::receive_from_t;
using recv_s = async_operation_t<M, raw_datagram_socket_t, recv_t>;
using recv_a = io_parameters_t<raw_datagram_socket_t, recv_t>;

io_result<receive_result> recv_s::submit(M& m, H const& h, C const& c, recv_s& s, recv_a const& a, io_handler<M>& handler)
{
	posix::socket_address_union& addr = new_address(s.address_storage);

	auto const buffers = a.buffers.buffers();
	msghdr& header = new_msghdr(s.header_storage) =
	{
		.msg_name = &addr.addr,
		.msg_namelen = sizeof(posix::socket_address_union),
		.msg_iov = reinterpret_cast<iovec*>(const_cast<read_buffer*>(buffers.data())),
		.msg_iovlen = buffers.size(),
	};

	io_uring_multiplexer::record_context ctx(m);
	auto const [fd, fd_flags] = ctx.get_fd(c, h.platform_handle);

	vsm_try_discard(ctx.push(
	{
		.opcode = IORING_OP_RECVMSG,
		.flags = fd_flags,
		.fd = fd,
		.addr = reinterpret_cast<uintptr_t>(&header),
		.user_data = ctx.get_user_data(handler),
	}));

	vsm_try_void(ctx.commit());

	return io_pending(error::async_operation_pending);
}

io_result<receive_result> recv_s::notify(M&, H const& h, C const&, recv_s& s, recv_a const& a, M::io_status_type const status)
{
	// This operation uses no io_slots.
	vsm_assert(status.slot == nullptr);

	if (status.result < 0)
	{
		return vsm::unexpected(static_cast<system_error>(-status.result));
	}

	posix::socket_address_union& addr = get_address(s.address_storage);

	return vsm_lazy(receive_result
	{
		.size = static_cast<size_t>(status.result),
		.endpoint = addr.get_network_endpoint(),
	});
}

void recv_s::cancel(M&, H const& h, C const&, recv_s& s)
{
	//TODO: Cancel
}
