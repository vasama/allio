#include <allio/win32/detail/iocp/raw_datagram_socket.hpp>

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
using H = native_handle<raw_datagram_socket_t>;
using C = async_connector_t<M, raw_datagram_socket_t>;

using bind_s = async_operation_t<M, raw_datagram_socket_t, bind_t>;
using bind_a = io_parameters_t<raw_datagram_socket_t, bind_t>;

io_result<void> bind_s::submit(M& m, H& h, C& c, bind_s&, bind_a const& a, io_handler<M>&)
{
	if (vsm::any_flags(a.flags, io_flags::create_synchronous))
	{
		return vsm::unexpected(allio_error(error::invalid_argument));
	}

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
	vsm_try_void(m.attach_platform_handle(posix::wrap_socket(socket.get()), c));

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
		},
	};

	return {};
}

io_result<void> bind_s::notify(
	M&,
	H&,
	C&,
	bind_s&,
	bind_a const&,
	io_handler<M>&, M::io_status_type)
{
	vsm_unreachable();
}

void bind_s::cancel(M&, H const&, C const&, bind_s&)
{
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

using send_t = raw_datagram_socket_t::send_to_t;
using send_s = async_operation_t<M, raw_datagram_socket_t, send_t>;
using send_a = io_parameters_t<raw_datagram_socket_t, send_t>;

io_result<void> send_s::submit(
	M& m,
	H const& h,
	C const&,
	send_s& s,
	send_a const& a,
	io_handler<M>& handler)
{
	vsm_try_void(check_wsa_buffers_size<DWORD>(a.buffers));

	io_extension_allocator extension = initialize_extension(s);

	vsm_try(addr, posix::get_socket_address(a.endpoint, extension));
	vsm_try(wsa_buffers, get_wsa_buffers(a.buffers, extension));

	DWORD transferred;

	OVERLAPPED& overlapped = *s.overlapped;
	overlapped.Pointer = nullptr;
	overlapped.hEvent = NULL;

	s.overlapped.bind(handler);

	vsm_try(already_completed, submit_socket_io(m, h, [&]() -> DWORD
	{
		if (win32::WSASendTo(
			posix::unwrap_socket(h.platform_handle),
			// This function is not const-correct.
			const_cast<WSABUF*>(wsa_buffers.data()),
			vsm::truncating(wsa_buffers.size()),
			&transferred,
			/* dwFlags: */ 0,
			addr.addr,
			addr.size,
			&overlapped,
			/* lpCompletionRoutine: */ nullptr) == SOCKET_ERROR)
		{
			return static_cast<DWORD>(WSAGetLastError());
		}
		return ERROR_SUCCESS;
	}));

	if (already_completed)
	{
		vsm_assert(transferred == get_io_buffers_size(a.buffers));
		return {};
	}

	extension.release();
	return vsm::unexpected(io_notify_status::submitted);
}

io_result<void> send_s::notify(
	M&,
	H const& h,
	C const&,
	send_s& s,
	send_a const& a,
	io_handler<M>& handler,
	M::io_status_type const status)
{
	io_extension_allocator const extension = acquire_extension(s);

	vsm_assert(&status.slot == &s.overlapped);

	if (!NT_SUCCESS(status.status))
	{
		return vsm::unexpected(allio_error(static_cast<kernel_error>(status.status)));
	}

	size_t const transferred = get_transfer_result(h, s.overlapped);
	vsm_assert(transferred == get_io_buffers_size(a.buffers));

	return {};
}

void send_s::cancel(M&, H const& h, C const&, send_s& s)
{
	cancel_socket_io(posix::unwrap_socket(h.platform_handle), *s.overlapped);
}


using recv_t = raw_datagram_socket_t::receive_from_t;
using recv_s = async_operation_t<M, raw_datagram_socket_t, recv_t>;
using recv_a = io_parameters_t<raw_datagram_socket_t, recv_t>;

io_result<size_t> recv_s::submit(
	M& m,
	H const& h,
	C const&,
	recv_s& s,
	recv_a const& a,
	io_handler<M>& handler)
{
	static_assert(std::is_same_v<
		decltype(recv_s::address_size),
		posix::socket_address_size_type>);

	vsm_try_void(check_wsa_buffers_size<DWORD>(a.buffers));

	io_extension_allocator extension = initialize_extension(s);

	vsm_try(wsa_buffers, get_wsa_buffers(a.buffers, extension));

	int const address_family = posix::get_address_family(h.flags);
	size_t const max_address_size = posix::get_max_socket_address_size(address_family);

	void* address_storage = nullptr;
	posix::socket_address_size_type* address_size = nullptr;

	if (a.endpoint)
	{
		if (a.endpoint.is_platform_endpoint())
		{
			vsm_try_assign(address_storage, a.endpoint.resize(
				max_address_size,
				max_address_size,
				std::align_val_t(alignof(posix::socket_address_union))));

			s.address_size = vsm::truncating(max_address_size);
			address_size = &s.address_size;
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

	OVERLAPPED& overlapped = *s.overlapped;
	overlapped.Pointer = nullptr;
	overlapped.hEvent = NULL;

	s.overlapped.bind(handler);

	DWORD transferred;
	DWORD flags = 0;

	vsm_try(already_completed, submit_socket_io(m, h, [&]() -> DWORD
	{
		if (win32::WSARecvFrom(
			posix::unwrap_socket(h.platform_handle),
			const_cast<WSABUF*>(wsa_buffers.data()),
			vsm::truncating(wsa_buffers.size()),
			&transferred,
			&flags,
			static_cast<sockaddr*>(address_storage),
			address_size,
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

io_result<size_t> recv_s::notify(
	M&,
	H const& h,
	C const&,
	recv_s& s,
	recv_a const&,
	io_handler<M>& handler,
	M::io_status_type const status)
{
	io_extension_allocator const extension = acquire_extension(s);

	vsm_assert(&status.slot == &s.overlapped);

	if (!NT_SUCCESS(status.status))
	{
		return vsm::unexpected(allio_error(static_cast<kernel_error>(status.status)));
	}

	size_t const transferred = get_transfer_result(h, s.overlapped);
	vsm_assert(transferred != 0);

	return transferred;
}

void recv_s::cancel(M&, H const& h, C const&, recv_s& s)
{
	cancel_socket_io(posix::unwrap_socket(h.platform_handle), *s.overlapped);
}
