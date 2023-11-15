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
using bind_i = operation_impl<M, H, bind_t>;
using bind_s = operation_t<M, H, bind_t>;

io_result2<void> bind_i::submit(M& m, N& h, C& c, bind_s& s)
{
	vsm_try(addr, posix::socket_address::make(s.args.endpoint));
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

	h = platform_object_t::native_type
	{
		{
			object_t::flags::not_null | flags,
		},
		posix::wrap_socket(socket.release()),
	};

	return {};
}

io_result2<void> bind_i::notify(M& m, N& h, C& c, bind_s& s, io_status const p_status)
{
	vsm_unreachable();
}

void bind_i::cancel(M& m, N const& h, C const& c, bind_s& s)
{
}


static size_t get_buffers_size(untyped_buffers const buffers)
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

using send_to_t = H::send_to_t;
using send_i = operation_impl<M, H, send_to_t>;
using send_s = operation_t<M, H, send_to_t>;

io_result2<void> send_i::submit(M& m, N const& h, C const& c, send_s& s)
{
	vsm_try(addr, posix::socket_address::make(s.args.endpoint));
	vsm_try(wsa_buffers, make_wsa_buffers(s.buffers, s.args.buffers.buffers()));

	DWORD transferred;

	OVERLAPPED& overlapped = *s.overlapped;
	overlapped.Pointer = nullptr;
	overlapped.hEvent = NULL;

	s.overlapped.bind(s);

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
		vsm_assert(transferred == get_buffers_size(s.args.buffers.buffers()));
		return {};
	}

	return io_pending;
}

io_result2<void> send_i::notify(M& m, N const& h, C const& c, send_s& s, io_status const p_status)
{
	auto const& status = M::unwrap_io_status(p_status);
	vsm_assert(&status.slot == &s.overlapped);

	if (!NT_SUCCESS(status.status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status.status));
	}

	size_t const transferred = get_transfer_result(h, s.overlapped);
	vsm_assert(transferred == get_buffers_size(s.args.buffers.buffers()));

	return {};
}

void send_i::cancel(M& m, N const& h, C const& c, send_s& s)
{
	cancel_socket_io(posix::unwrap_socket(h.platform_handle), *s.overlapped);
}


using receive_from_t = H::receive_from_t;
using receive_i = operation_impl<M, H, receive_from_t>;
using receive_s = operation_t<M, H, receive_from_t>;

io_result2<receive_result> receive_i::submit(M& m, N const& h, C const& c, receive_s& s)
{
	vsm_try(wsa_buffers, make_wsa_buffers(s.buffers, s.args.buffers.buffers()));

	DWORD transferred;
	DWORD flags = 0;

	OVERLAPPED& overlapped = *s.overlapped;
	overlapped.Pointer = nullptr;
	overlapped.hEvent = NULL;

	auto& addr_buffer = new_wsa_address_buffer<posix::socket_address>(s.address_storage);
	addr_buffer.size = sizeof(posix::socket_address_union);

	s.overlapped.bind(s);

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

	return io_pending;
}

io_result2<receive_result> receive_i::notify(M& m, N const& h, C const& c, receive_s& s, io_status const p_status)
{
	auto const& status = M::unwrap_io_status(p_status);
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

void receive_i::cancel(M& m, N const& h, C const& c, receive_s& s)
{
	cancel_socket_io(posix::unwrap_socket(h.platform_handle), *s.overlapped);
}
