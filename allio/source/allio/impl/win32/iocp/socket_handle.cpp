#include <allio/win32/detail/iocp/socket_handle.hpp>

#include <allio/impl/posix/socket.hpp>
#include <allio/impl/win32/iocp/socket.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/impl/win32/wsa.hpp>
#include <allio/impl/win32/wsa_ex.hpp>
#include <allio/win32/kernel_error.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

using M = iocp_multiplexer;
using H = raw_socket_handle_t;
using N = H::native_type;
using C = connector_t<M, H>;


using connect_t = H::connect_t;
using connect_i = operation_impl<M, H, connect_t>;
using connect_s = operation_t<M, H, connect_t>;

static H::native_type make_native_handle(unique_wrapped_socket& socket, handle_flags const flags)
{
	return platform_object_t::native_type
	{
		{
			H::flags::not_null | flags,
		},
		socket.release(),
	};

	return {};
}

io_result2<void> connect_i::submit(M& m, N& h, C& c, connect_s& s)
{
	vsm_try(addr, posix::socket_address::make(s.args.endpoint));

	vsm_try(socket, posix::create_socket(
		addr.addr.sa_family,
		/* multiplexable: */ true));

	vsm_try_void(m.attach_handle(posix::wrap_socket(socket.socket.get()), c));

	// The socket must be bound before calling ConnectEx.
	{
		posix::socket_address_union bind_addr;
		memset(&bind_addr, 0, sizeof(bind_addr));
		bind_addr.addr.sa_family = addr.addr.sa_family;

		if (::bind(socket.socket.get(), &bind_addr.addr, addr.size) == SOCKET_ERROR)
		{
			return vsm::unexpected(posix::get_last_socket_error());
		}
	}

	s.socket = unique_wrapped_socket(posix::wrap_socket(socket.socket.release()));
	s.socket_flags = socket.flags;

	OVERLAPPED& overlapped = *s.overlapped;
	overlapped.Pointer = nullptr;
	overlapped.hEvent = NULL;

	s.overlapped.bind(s);

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
		h = make_native_handle(s.socket, s.socket_flags);
		return {};
	}

	return io_pending;
}

io_result2<void> connect_i::notify(M& m, N& h, C& c, connect_s& s, io_status const p_status)
{
	auto const& status = M::unwrap_io_status(p_status);
	vsm_assert(&status.slot == &s.overlapped);

	if (!NT_SUCCESS(status.status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status.status));
	}

	h = make_native_handle(s.socket, s.socket_flags);
	return {};
}

void connect_i::cancel(M& m, N const& h, C const& c, S& s)
{
	cancel_socket_io(posix::unwrap_socket(s.socket.get()), *s.overlapped);
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

	vsm_assert(transferred != 0);
	vsm_assert(flags == 0);

	return transferred;
}

using read_t = H::read_some_t;
using read_i = operation_impl<M, H, read_t>;
using read_s = operation_t<M, H, read_t>;

io_result2<size_t> read_i::submit(M& m, N const& h, C const& c, read_s& s)
{
	vsm_try(wsa_buffers, make_wsa_buffers(s.buffers, s.args.buffers.buffers()));

	DWORD transferred;
	DWORD flags = 0;

	OVERLAPPED& overlapped = *s.overlapped;
	overlapped.Pointer = nullptr;
	overlapped.hEvent = NULL;

	s.overlapped.bind(s);

	// If using a multithreaded completion port, after this call
	// another thread will race to complete this operation.
	vsm_try(already_completed, submit_socket_io(m, h, [&]()
	{
		if (WSARecv(
			posix::unwrap_socket(h.platform_handle),
			wsa_buffers.data(),
			wsa_buffers.size(),
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

	return io_pending;
}

io_result2<size_t> read_i::notify(M& m, N const& h, C const& c, read_s& s, io_status const p_status)
{
	auto const& status = M::unwrap_io_status(p_status);
	vsm_assert(&status.slot == &s.overlapped);

	if (!NT_SUCCESS(status.status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status.status));
	}

	return get_transfer_result(h, s.overlapped);
}

void read_i::cancel(M& m, N const& h, C const& c, read_s& s)
{
}



using write_t = H::write_some_t;
using write_i = operation_impl<M, H, write_t>;
using write_s = operation_t<M, H, write_t>;

io_result2<size_t> write_i::submit(M& m, N const& h, C const& c, write_s& s)
{
	vsm_try(wsa_buffers, make_wsa_buffers(s.buffers, s.args.buffers.buffers()));

	DWORD transferred;

	OVERLAPPED& overlapped = *s.overlapped;
	overlapped.Pointer = nullptr;
	overlapped.hEvent = NULL;

	s.overlapped.bind(s);

	// If using a multithreaded completion port, after this call
	// another thread will race to complete this operation.
	vsm_try(already_completed, submit_socket_io(m, h, [&]()
	{
		if (WSASend(
			posix::unwrap_socket(h.platform_handle),
			wsa_buffers.data(),
			wsa_buffers.size(),
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

	return io_pending;
}

io_result2<size_t> write_i::notify(M& m, N const& h, C const& c, write_s& s, io_status const p_status)
{
	auto const& status = M::unwrap_io_status(p_status);
	vsm_assert(&status.slot == &s.overlapped);

	if (!NT_SUCCESS(status.status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status.status));
	}

	return get_transfer_result(h, s.overlapped);
}

void write_i::cancel(M& m, N const& h, C const& c, write_s& s)
{
}
