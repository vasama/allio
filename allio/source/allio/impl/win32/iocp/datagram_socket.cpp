#include <allio/win32/detail/iocp/datagram_socket.hpp>

#include <allio/impl/posix/socket.hpp>
#include <allio/impl/win32/iocp/socket.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/impl/win32/wsa.hpp>
#include <allio/win32/kernel_error.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

using M = iocp_multiplexer;
using H = raw_datagram_socket_t;
using N = H::native_type;
using C = connector_t<M, H>;


using bind_t = H::bind_t;
using bind_i = operation<M, H, bind_t>;
using bind_s = operation_t<M, H, bind_t>;
using bind_a = io_parameters_t<bind_t>;

io_result<void> bind_i::submit(M& m, N& h, C& c, bind_s& s, bind_a const& args, io_handler<M>& handler)
{
	vsm_try(addr, posix::socket_address::make(args.endpoint));
	vsm_try(protocol, posix::choose_protocol(addr.addr.sa_family, SOCK_DGRAM));

	vsm_try_bind((socket, flags), posix::create_socket(
		addr.addr.sa_family,
		//TODO: Add raw protocol support
		SOCK_DGRAM,
		protocol,
		/* multiplexable: */ true));

	vsm_try_void(socket_bind(socket.get(), addr));

	vsm_try_void(m.attach_handle(
		posix::wrap_socket(socket.get()),
		c));

	h = N
	{
		platform_object_t::native_type
		{
			{
				object_t::flags::not_null | flags,
			},
			posix::wrap_socket(socket.release()),
		}
	};

	return {};
}

io_result<void> bind_i::notify(M& m, N& h, C& c, bind_s& s, bind_a const& args, M::io_status_type const status)
{
	vsm_unreachable();
}

void bind_i::cancel(M& m, N const& h, C const& c, bind_s& s)
{
}


template<vsm::any_cv_of<std::byte> T>
static size_t get_buffers_size(basic_buffers<T> const buffers)
{
	size_t size = 0;
	for (auto const& buffer : buffers)
	{
		size += buffer.size();
	}
	return size;
}

static size_t get_transfer_result(N const& h, M::overlapped& overlapped)
{
	DWORD transferred;
	DWORD flags;

	vsm_verify(WSAGetOverlappedResult(
		posix::unwrap_socket(h.platform_handle),
		overlapped.get(),
		&transferred,
		/* fWait: */ false,
		&flags));

	vsm_assert(flags == 0);

	return transferred;
}

using send_t = H::send_to_t;
using send_i = operation<M, H, send_t>;
using send_s = operation_t<M, H, send_t>;
using send_a = io_parameters_t<send_t>;

io_result<void> send_i::submit(M& m, N const& h, C const& c, send_s& s, send_a const& args, io_handler<M>& handler)
{
	vsm_try(addr, posix::socket_address::make(args.endpoint));
	vsm_try(wsa_buffers, make_wsa_buffers(s.buffers, args.buffers.buffers()));

	DWORD transferred;

	OVERLAPPED& overlapped = *s.overlapped;
	overlapped.Pointer = nullptr;
	overlapped.hEvent = NULL;

	s.overlapped.bind(handler);

	vsm_try(already_completed, submit_socket_io(m, h, [&]()
	{
		if (WSASendTo(
			posix::unwrap_socket(h.platform_handle),
			wsa_buffers.data(),
			wsa_buffers.size(),
			&transferred,
			/* dwFlags: */ 0,
			&addr.addr,
			addr.size,
			&overlapped,
			/* lpCompletionRoutine: */ nullptr) == SOCKET_ERROR)
		{
			return WSAGetLastError();
		}
		return 0;
	}));

	if (already_completed)
	{
		vsm_assert(transferred == get_buffers_size(args.buffers.buffers()));
		return {};
	}

	return io_pending(error::async_operation_pending);
}

io_result<void> send_i::notify(M& m, N const& h, C const& c, send_s& s, send_a const& args, M::io_status_type const status)
{
	vsm_assert(&status.slot == &s.overlapped);

	if (!NT_SUCCESS(status.status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status.status));
	}

	size_t const transferred = get_transfer_result(h, s.overlapped);
	vsm_assert(transferred == get_buffers_size(args.buffers.buffers()));

	return {};
}

void send_i::cancel(M& m, N const& h, C const& c, send_s& s)
{
	cancel_socket_io(posix::unwrap_socket(h.platform_handle), *s.overlapped);
}


using recv_t = H::receive_from_t;
using recv_i = operation<M, H, recv_t>;
using recv_s = operation_t<M, H, recv_t>;
using recv_a = io_parameters_t<recv_t>;

io_result<receive_result> recv_i::submit(M& m, N const& h, C const& c, recv_s& s, recv_a const& args, io_handler<M>& handler)
{
	vsm_try(wsa_buffers, make_wsa_buffers(s.buffers, args.buffers.buffers()));

	DWORD transferred;
	DWORD flags = 0;

	OVERLAPPED& overlapped = *s.overlapped;
	overlapped.Pointer = nullptr;
	overlapped.hEvent = NULL;

	auto& addr_buffer = new_wsa_address_buffer<posix::socket_address>(s.address_storage);
	addr_buffer.size = sizeof(posix::socket_address_union);

	s.overlapped.bind(handler);

	vsm_try(already_completed, submit_socket_io(m, h, [&]()
	{
		if (WSARecvFrom(
			posix::unwrap_socket(h.platform_handle),
			wsa_buffers.data(),
			wsa_buffers.size(),
			&transferred,
			&flags,
			&addr_buffer.addr,
			&addr_buffer.size,
			&overlapped,
			/* lpCompletionRoutine: */ nullptr) == SOCKET_ERROR)
		{
			return WSAGetLastError();
		}
		return 0;
	}));

	if (already_completed)
	{
		return vsm_lazy(receive_result
		{
			.size = transferred,
			.endpoint = addr_buffer.get_network_endpoint(),
		});
	}

	return io_pending(error::async_operation_pending);
}

io_result<receive_result> recv_i::notify(M& m, N const& h, C const& c, recv_s& s, recv_a const& args, M::io_status_type const status)
{
	vsm_assert(&status.slot == &s.overlapped);

	if (!NT_SUCCESS(status.status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status.status));
	}

	size_t const transferred = get_transfer_result(h, s.overlapped);
	vsm_assert(transferred != 0);

	auto& addr_buffer = get_wsa_address_buffer<posix::socket_address>(s.address_storage);

	return vsm_lazy(receive_result
	{
		.size = transferred,
		.endpoint = addr_buffer.get_network_endpoint(),
	});
}

void recv_i::cancel(M& m, N const& h, C const& c, recv_s& s)
{
	cancel_socket_io(posix::unwrap_socket(h.platform_handle), *s.overlapped);
}
