#include <allio/win32/detail/iocp/raw_socket.hpp>

#include <allio/impl/posix/socket.hpp>
#include <allio/impl/win32/iocp/raw_socket.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/impl/win32/wsa.hpp>
#include <allio/win32/kernel_error.hpp>

#include <vsm/numeric.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

using M = iocp_multiplexer;
using H = native_handle<raw_socket_t>;
using C = async_connector_t<M, raw_socket_t>;

using connect_t = raw_socket_t::connect_t;
using connect_s = async_operation_t<M, raw_socket_t, connect_t>;
using connect_a = io_parameters_t<raw_socket_t, connect_t>;

io_result<void> connect_s::submit(M& m, H& h, C& c, connect_s& s, connect_a const& a, io_handler<M>& handler)
{
	vsm_try(addr, posix::socket_address::make(a.endpoint));
	vsm_try(protocol, posix::choose_protocol(addr.addr.sa_family, SOCK_STREAM));

	vsm_try_bind((socket, flags), posix::create_socket(
		addr.addr.sa_family,
		SOCK_STREAM,
		protocol,
		a.flags | io_flags::create_non_blocking));

	vsm_try_void(m.attach_handle(posix::wrap_socket(socket.get()), c));

	// The socket must be bound before calling ConnectEx.
	{
		posix::socket_address_union bind_addr;
		memset(&bind_addr, 0, sizeof(bind_addr));
		bind_addr.addr.sa_family = addr.addr.sa_family;

		vsm_try_void(socket_bind(socket.get(), bind_addr, addr.size));
	}

	s.socket = unique_wrapped_socket(posix::wrap_socket(socket.release()));
	s.socket_flags = flags;

	OVERLAPPED& overlapped = *s.overlapped;
	overlapped.Pointer = nullptr;
	overlapped.hEvent = NULL;

	s.overlapped.bind(handler);

	// If using a multithreaded completion port, after this call
	// another thread will race to complete this operation.
	vsm_try(already_connected, submit_socket_io(m, h, [&]()
	{
		return wsa_connect_ex(
			posix::unwrap_socket(s.socket.get()),
			addr,
			overlapped);
	}));

	if (already_connected)
	{
		h.flags = object_t::flags::not_null | s.socket_flags;
		h.platform_handle = s.socket.release();

		return {};
	}

	return io_pending(error::async_operation_pending);
}

io_result<void> connect_s::notify(M&, H& h, C&, connect_s& s, connect_a const&, M::io_status_type const status)
{
	vsm_assert(&status.slot == &s.overlapped);

	if (!NT_SUCCESS(status.status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status.status));
	}

	h.flags = object_t::flags::not_null | s.socket_flags;
	h.platform_handle = s.socket.release();

	return {};
}

void connect_s::cancel(M&, H const&, C const&, S& s)
{
	cancel_socket_io(posix::unwrap_socket(s.socket.get()), *s.overlapped);
}


static size_t get_transfer_result(H const& h, M::overlapped& overlapped)
{
	DWORD transferred;
	DWORD flags;

	vsm_verify(WSAGetOverlappedResult(
		posix::unwrap_socket(h.platform_handle),
		overlapped.get(),
		&transferred,
		/* fWait: */ false,
		&flags));

	vsm_assert(transferred != 0);
	vsm_assert(flags == 0);

	return transferred;
}

using read_t = raw_socket_t::stream_read_t;
using read_s = async_operation_t<M, raw_socket_t, read_t>;
using read_a = io_parameters_t<raw_socket_t, read_t>;

io_result<size_t> read_s::submit(M& m, H const& h, C const&, read_s& s, read_a const& a, io_handler<M>& handler)
{
	vsm_try(wsa_buffers, make_wsa_buffers(s.buffers, a.buffers.buffers()));

	DWORD transferred;
	DWORD flags = 0;

	OVERLAPPED& overlapped = *s.overlapped;
	overlapped.Pointer = nullptr;
	overlapped.hEvent = NULL;

	s.overlapped.bind(handler);

	// If using a multithreaded completion port, after this call
	// another thread will race to complete this operation.
	vsm_try(already_completed, submit_socket_io(m, h, [&]()
	{
		if (win32::WSARecv(
			posix::unwrap_socket(h.platform_handle),
			wsa_buffers.data,
			wsa_buffers.size,
			&transferred,
			&flags,
			&overlapped,
			/* lpCompletionRoutine: */ nullptr) == SOCKET_ERROR)
		{
			return WSAGetLastError();
		}
		return 0;
	}));

	if (already_completed)
	{
		vsm_assert(flags == 0);
		return transferred;
	}

	return io_pending(error::async_operation_pending);
}

io_result<size_t> read_s::notify(M&, H const& h, C const&, read_s& s, read_a const&, M::io_status_type const status)
{
	vsm_assert(&status.slot == &s.overlapped);

	if (!NT_SUCCESS(status.status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status.status));
	}

	return get_transfer_result(h, s.overlapped);
}

void read_s::cancel(M&, H const& h, C const&, read_s& s)
{
	cancel_socket_io(posix::unwrap_socket(h.platform_handle), *s.overlapped);
}


using write_t = raw_socket_t::stream_write_t;
using write_s = async_operation_t<M, raw_socket_t, write_t>;
using write_a = io_parameters_t<raw_socket_t, write_t>;

io_result<size_t> write_s::submit(M& m, H const& h, C const&, write_s& s, write_a const& a, io_handler<M>& handler)
{
	vsm_try(wsa_buffers, make_wsa_buffers(s.buffers, a.buffers.buffers()));

	DWORD transferred;

	OVERLAPPED& overlapped = *s.overlapped;
	overlapped.Pointer = nullptr;
	overlapped.hEvent = NULL;

	s.overlapped.bind(handler);

	// If using a multithreaded completion port, after this call
	// another thread will race to complete this operation.
	vsm_try(already_completed, submit_socket_io(m, h, [&]()
	{
		if (win32::WSASend(
			posix::unwrap_socket(h.platform_handle),
			wsa_buffers.data,
			wsa_buffers.size,
			&transferred,
			/* dwFlags: */ 0,
			&overlapped,
			/* lpCompletionRoutine: */ nullptr) == SOCKET_ERROR)
		{
			return WSAGetLastError();
		}
		return 0;
	}));

	if (already_completed)
	{
		return transferred;
	}

	return io_pending(error::async_operation_pending);
}

io_result<size_t> write_s::notify(M&, H const& h, C const&, write_s& s, write_a const&, M::io_status_type const status)
{
	vsm_assert(&status.slot == &s.overlapped);

	if (!NT_SUCCESS(status.status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status.status));
	}

	return get_transfer_result(h, s.overlapped);
}

void write_s::cancel(M&, H const& h, C const&, write_s& s)
{
	cancel_socket_io(posix::unwrap_socket(h.platform_handle), *s.overlapped);
}
