#include <allio/win32/detail/iocp/raw_socket.hpp>

#include <allio/impl/byte_io_buffers.hpp>
#include <allio/impl/io_extension.hpp>
#include <allio/impl/posix/handles/raw_common_socket.hpp>
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

io_result<void> connect_s::submit(
	M& m,
	H& h,
	C& c,
	connect_s& s,
	connect_a const& a,
	io_handler<M>& handler)
{
	if (vsm::any_flags(a.flags, io_flags::create_synchronous))
	{
		return vsm::unexpected(allio_error(error::invalid_argument));
	}

	io_extension_allocator extension = initialize_extension(s);

	//TODO: Check the lifetime requirements of the socket address.
	vsm_try(addr, posix::get_socket_address(a.endpoint, extension));
	vsm_try(protocol, posix::choose_protocol(addr.addr->sa_family, SOCK_STREAM));

	vsm_try_bind((socket, flags), posix::create_socket(
		addr.addr->sa_family,
		SOCK_STREAM,
		protocol,
		a.flags));

	vsm_try_void(m.attach_platform_handle(posix::wrap_socket(socket.get()), c));

	// The socket must be bound before calling ConnectEx.
	{
		posix::socket_address_union bind_addr;
		memset(&bind_addr, 0, sizeof(bind_addr));
		bind_addr.addr.sa_family = addr.addr->sa_family;

		vsm_try_void(posix::socket_bind(socket.get(), &bind_addr.addr, addr.size));
	}

	s.socket = unique_wrapped_socket(posix::wrap_socket(socket.release()));
	s.socket_flags = flags | posix::set_address_family(addr.addr->sa_family);

	OVERLAPPED& overlapped = *s.overlapped;
	overlapped.Pointer = nullptr;
	overlapped.hEvent = NULL;

	s.overlapped.bind(handler);

	// If using a multithreaded completion port, after this call another thread will race to
	// complete this operation.
	vsm_try(already_completed, submit_socket_io(m, h, [&]()
	{
		return wsa_connect_ex(
			posix::unwrap_socket(s.socket.get()),
			addr.addr,
			addr.size,
			&overlapped);
	}));

#if 0
		DWORD transferred = static_cast<DWORD>(-1);
		bool const result = win32::ConnectEx(
			posix::unwrap_socket(s.socket.get()),
			addr.addr,
			addr.size,
			/* lpSendBuffer: */ nullptr,
			/* dwSendDataLength: */ 0,
			&transferred,
			&overlapped);

		if (!result)
		{
			return static_cast<DWORD>(WSAGetLastError());
		}

		vsm_assert(transferred == 0);
		return 0;
#endif

	if (already_completed)
	{
		h.flags = object_t::flags::not_null | s.socket_flags;
		h.platform_handle = s.socket.release();

		return {};
	}

	extension.release();
	return vsm::unexpected(io_notify_status::submitted);
}

io_result<void> connect_s::notify(
	M&,
	H& h,
	C&,
	connect_s& s,
	connect_a const&,
	io_handler<M>& handler,
	M::io_status_type const status)
{
	io_extension_allocator const extension = acquire_extension(s);

	vsm_assert(&status.slot == &s.overlapped);

	if (!NT_SUCCESS(status.status))
	{
		return vsm::unexpected(allio_error(static_cast<kernel_error>(status.status)));
	}

	h.flags = object_t::flags::not_null | s.socket_flags;
	h.platform_handle = s.socket.release();

	return {};
}

void connect_s::cancel(M&, H const&, C const&, S& s)
{
	cancel_socket_io(posix::unwrap_socket(s.socket.get()), *s.overlapped);
}


using disconnect_s = async_operation_t<M, raw_socket_t, disconnect_t>;
using disconnect_a = io_parameters_t<raw_socket_t, disconnect_t>;

io_result<void> disconnect_s::submit(
	M& m,
	H const& h,
	C const& c,
	disconnect_s& s,
	disconnect_a const& a,
	io_handler<M>& handler)
{
	SOCKET const socket = posix::unwrap_socket(h.platform_handle);

	OVERLAPPED& overlapped = *s.overlapped;
	overlapped.Pointer = nullptr;
	overlapped.hEvent = NULL;

	s.overlapped.bind(handler);

	// If using a multithreaded completion port, after this call another thread will race to
	// complete this operation.
	vsm_try(already_completed, submit_socket_io(m, h, [&]()
	{
		return wsa_disconnect_ex(socket, /* flags: */ 0, &overlapped);
	}));

	if (already_completed)
	{
		return {};
	}

	return vsm::unexpected(io_notify_status::submitted);
}

io_result<void> disconnect_s::notify(
	M& m,
	H const& h,
	C const& c,
	disconnect_s& s,
	disconnect_a const& a,
	io_handler<M>&,
	M::io_status_type const status)
{
	vsm_assert(&status.slot == &s.overlapped);

	if (!NT_SUCCESS(status.status))
	{
		return vsm::unexpected(allio_error(static_cast<kernel_error>(status.status)));
	}

	return {};
}

void disconnect_s::cancel(M& m, H const&, C const&, S& s)
{
	vsm_unreachable();
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

	vsm_assert(flags == 0);

	return transferred;
}

using recv_t = raw_socket_t::stream_read_t;
using recv_s = async_operation_t<M, raw_socket_t, recv_t>;
using recv_a = io_parameters_t<raw_socket_t, recv_t>;

static vsm::result<read_result> get_read_transferred(recv_a const& a, size_t const transferred)
{
	if (transferred == 0 && !io_buffers_is_empty(a.buffers))
	{
		return end_of_stream;
	}

	return transferred;
}

io_result<read_result> recv_s::submit(
	M& m,
	H const& h,
	C const&,
	recv_s& s,
	recv_a const& a,
	io_handler<M>& handler)
{
	vsm_try_void(check_wsa_buffers_size<DWORD>(a.buffers));

	io_extension_allocator extension = initialize_extension(s);
	vsm_try(wsa_buffers, get_wsa_buffers(a.buffers, extension));

	DWORD transferred;
	DWORD flags = 0;

	OVERLAPPED& overlapped = *s.overlapped;
	overlapped.Pointer = nullptr;
	overlapped.hEvent = NULL;

	s.overlapped.bind(handler);

	// If using a multithreaded completion port, after this call another thread will race to
	// complete this operation.
	vsm_try(already_completed, submit_socket_io(m, h, [&]() -> DWORD
	{
		if (win32::WSARecv(
			posix::unwrap_socket(h.platform_handle),
			const_cast<WSABUF*>(wsa_buffers.data()),
			vsm::truncating(wsa_buffers.size()),
			&transferred,
			&flags,
			&overlapped,
			/* lpCompletionRoutine: */ nullptr) == SOCKET_ERROR)
		{
			return static_cast<DWORD>(WSAGetLastError());
		}
		return ERROR_SUCCESS;
	}));

	if (already_completed)
	{
		vsm_assert(flags == 0);
		return get_read_transferred(a, transferred);
	}

	extension.release();
	return vsm::unexpected(io_notify_status::submitted);
}

io_result<read_result> recv_s::notify(
	M&,
	H const& h,
	C const&,
	recv_s& s,
	recv_a const& a,
	io_handler<M>& handler,
	M::io_status_type const status)
{
	io_extension_allocator const extension = acquire_extension(s);

	vsm_assert(&status.slot == &s.overlapped);

	if (!NT_SUCCESS(status.status))
	{
		return vsm::unexpected(allio_error(static_cast<kernel_error>(status.status)));
	}

	return get_read_transferred(a, get_transfer_result(h, s.overlapped));
}

void recv_s::cancel(M&, H const& h, C const&, recv_s& s)
{
	cancel_socket_io(posix::unwrap_socket(h.platform_handle), *s.overlapped);
}


using send_t = raw_socket_t::stream_write_t;
using send_s = async_operation_t<M, raw_socket_t, send_t>;
using send_a = io_parameters_t<raw_socket_t, send_t>;

io_result<size_t> send_s::submit(
	M& m,
	H const& h,
	C const&,
	send_s& s,
	send_a const& a,
	io_handler<M>& handler)
{
	vsm_try_void(check_wsa_buffers_size<DWORD>(a.buffers));

	io_extension_allocator extension = initialize_extension(s);
	vsm_try(wsa_buffers, get_wsa_buffers(a.buffers, extension));

	DWORD transferred;

	OVERLAPPED& overlapped = *s.overlapped;
	overlapped.Pointer = nullptr;
	overlapped.hEvent = NULL;

	s.overlapped.bind(handler);

	// If using a multithreaded completion port, after this call another thread will race to
	// complete this operation.
	vsm_try(already_completed, submit_socket_io(m, h, [&]() -> DWORD
	{
		if (win32::WSASend(
			posix::unwrap_socket(h.platform_handle),
			const_cast<WSABUF*>(wsa_buffers.data()),
			vsm::truncating(wsa_buffers.size()),
			&transferred,
			/* dwFlags: */ 0,
			&overlapped,
			/* lpCompletionRoutine: */ nullptr) == SOCKET_ERROR)
		{
			return static_cast<DWORD>(WSAGetLastError());
		}
		return ERROR_SUCCESS;
	}));

	if (already_completed)
	{
		return transferred;
	}

	extension.release();
	return vsm::unexpected(io_notify_status::submitted);
}

io_result<size_t> send_s::notify(
	M&,
	H const& h,
	C const&,
	send_s& s,
	send_a const&,
	io_handler<M>& handler,
	M::io_status_type const status)
{
	io_extension_allocator const extension = acquire_extension(s);

	vsm_assert(&status.slot == &s.overlapped);

	if (!NT_SUCCESS(status.status))
	{
		return vsm::unexpected(allio_error(static_cast<kernel_error>(status.status)));
	}

	return get_transfer_result(h, s.overlapped);
}

void send_s::cancel(M&, H const& h, C const&, send_s& s)
{
	cancel_socket_io(posix::unwrap_socket(h.platform_handle), *s.overlapped);
}
