#include <allio/linux/detail/io_uring/raw_datagram_socket.hpp>

#include <allio/impl/linux/byte_io.hpp>
#include <allio/impl/posix/handles/raw_common_socket.hpp>
#include <allio/linux/io_uring_record_context.hpp>

#include <vsm/numeric.hpp>

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

	posix::socket_address_storage address_storage;
	vsm_try(addr, posix::get_socket_address(a.endpoint, address_storage));
	vsm_try(protocol, posix::choose_protocol(addr.addr->sa_family, SOCK_DGRAM));

	vsm_try_bind((socket, flags), posix::create_socket(
		addr.addr->sa_family,
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
				object_t::flags::not_null
					| flags
					| posix::set_address_family(addr.addr->sa_family),
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


using send_t = send_to_t;
using send_s = async_operation_t<M, raw_datagram_socket_t, send_t>;
using send_a = io_parameters_t<raw_datagram_socket_t, send_t>;

io_result<void> send_s::submit(
	M& m,
	H const& h,
	C const& c,
	send_s& s,
	send_a const& a,
	io_handler<M>& handler)
{
	vsm_try_void(check_io_vectors_size(a.buffers));

	io_extension_allocator extension = initialize_extension(s);
	vsm_try(addr, posix::get_socket_address(a.endpoint, extension));
	vsm_try(io_vectors, get_io_vectors(a.buffers, extension));

	msghdr& header = new_msghdr(s.header_storage) =
	{
		.msg_name = const_cast<sockaddr*>(addr.addr),
		.msg_namelen = addr.size,
		// msghdr::msg_iov seems to be non-const-correct.
		.msg_iov = const_cast<iovec*>(io_vectors.data()),
		.msg_iovlen = io_vectors.size(),
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

	s.set_handler(handler);
	ctx.commit();
	extension.release();

	return vsm::unexpected(io_notify_status::submitted);
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
	io_extension_allocator const extension = acquire_extension(s);

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


using recv_t = receive_from_t;
using recv_s = async_operation_t<M, raw_datagram_socket_t, recv_t>;
using recv_a = io_parameters_t<raw_datagram_socket_t, recv_t>;

io_result<size_t> recv_s::submit(
	M& m,
	H const& h,
	C const& c,
	recv_s& s,
	recv_a const& a,
	io_handler<M>& handler)
{
	vsm_try_void(check_io_vectors_size(a.buffers));

	int const address_family = posix::get_address_family(h.flags);
	size_t const max_address_size = posix::get_max_socket_address_size(address_family);

	io_extension_allocator extension = initialize_extension(s);

	void* address_storage = nullptr;
	if (a.endpoint)
	{
		if (a.endpoint.is_platform_endpoint())
		{
			vsm_try_assign(address_storage, a.endpoint.resize(
				max_address_size,
				max_address_size,
				std::align_val_t(alignof(posix::socket_address_union))));
		}
		else if (a.endpoint.kind() != posix::get_address_kind(address_family))
		{
			return vsm::unexpected(allio_error(error::invalid_argument));
		}
		else
		{
			//TODO: Implement typed endpoint buffer usage.
			return vsm::unexpected(allio_error(error::unsupported_operation));
		}
	}

	vsm_try(io_vectors, get_io_vectors(a.buffers, extension));

	msghdr& header = new_msghdr(s.header_storage) =
	{
		.msg_name = address_storage,
		.msg_namelen = vsm::truncating(max_address_size),
		// msghdr::msg_iov seems to be non-const-correct.
		.msg_iov = const_cast<iovec*>(io_vectors.data()),
		.msg_iovlen = io_vectors.size(),
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

	return static_cast<size_t>(status.result);
}

void recv_s::cancel(M&, H const& h, C const&, recv_s& s)
{
	//TODO: Cancel
}
