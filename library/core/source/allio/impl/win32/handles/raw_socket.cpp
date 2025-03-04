#include <allio/detail/handles/raw_socket.hpp>

#include <allio/impl/posix/socket.hpp>
#include <allio/impl/win32/wsa_thread_event.hpp>
#include <allio/impl/win32/wsa.hpp>

#include <vsm/numeric.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

static constexpr auto max_buffer_count = std::numeric_limits<DWORD>::max();

byte_io_limits raw_socket_t::get_byte_io_limits(native_handle<raw_socket_t> const& h)
{
	return
	{
		//TODO: I/O functions should check this limit.
		.max_buffer_count = max_buffer_count,
		.max_atomic_buffer_count = max_buffer_count,
	};
}

vsm::result<void> raw_socket_t::connect(
	native_handle<raw_socket_t>& h,
	io_parameters_t<raw_socket_t, connect_t> const& a)
{
	if (a.deadline != deadline::never() &&
		h.flags[platform_object_t::impl_type::flags::synchronous])
	{
		return vsm::unexpected(allio_error(error::unsupported_operation));
	}

	posix::socket_address_storage address_storage;
	vsm_try(addr, posix::get_socket_address(a.endpoint, address_storage));
	vsm_try(protocol, posix::choose_protocol(addr.addr->sa_family, SOCK_STREAM));

	vsm_try_bind((socket, flags), posix::create_socket(
		addr.addr->sa_family,
		SOCK_STREAM,
		protocol,
		a.flags));

	if (vsm::no_flags(a.flags, io_flags::create_synchronous) || a.deadline != deadline::never())
	{
		//TODO: Test this and the matching accept with attached IOCP.
		vsm_try(overlapped, wsa_thread_overlapped::get_for(h));

		// The socket must be bound before calling ConnectEx.
		{
			posix::socket_address_union bind_addr;
			memset(&bind_addr, 0, sizeof(bind_addr));
			bind_addr.addr.sa_family = addr.addr->sa_family;

			vsm_try_void(posix::socket_bind(socket.get(), &bind_addr.addr, addr.size));
		}

		DWORD transferred = static_cast<DWORD>(-1);
		if (!win32::ConnectEx(
			socket.get(),
			addr.addr,
			addr.size,
			/* lpSendBuffer: */ nullptr,
			/* dwSendDataLength: */ 0,
			&transferred,
			overlapped))
		{
			if (int const e = WSAGetLastError(); e != WSA_IO_PENDING)
			{
				return vsm::unexpected(allio_error(static_cast<posix::socket_error>(e)));
			}

			DWORD connect_flags;
			vsm_try_void(overlapped.wait(
				socket.get(),
				a.deadline,
				&transferred,
				&connect_flags));
		}
		vsm_assert(transferred == 0);
	}
	else
	{
		if (::connect(socket.get(), addr.addr, addr.size) == SOCKET_ERROR)
		{
			return vsm::unexpected(allio_error(posix::get_last_socket_error()));
		}
	}

	h.flags = flags::not_null | flags;
	h.platform_handle = posix::wrap_socket(socket.release());

	return {};
}

vsm::result<size_t> raw_socket_t::stream_read(
	native_handle<raw_socket_t> const& h,
	io_parameters_t<raw_socket_t, stream_read_t> const& a)
{
	//TODO: Unify buffer count checks with check_wsa_buffers_size everywhere.
	//      Convert buffer count saturation to truncation.
	if (a.buffers.get_buffers_size() > max_buffer_count)
	{
		return vsm::unexpected(allio_error(error::too_many_io_buffers));
	}

	if (a.deadline != deadline::never() &&
		h.flags[platform_object_t::impl_type::flags::synchronous])
	{
		return vsm::unexpected(allio_error(error::unsupported_operation));
	}

	SOCKET const socket = posix::unwrap_socket(h.platform_handle);

	automatic_wsa_buffer_storage buffer_storage;
	vsm_try(wsa_buffers, get_wsa_buffers(a.buffers, buffer_storage));

	vsm_try(overlapped, wsa_thread_overlapped::get_for(h));

	DWORD transferred;
	DWORD flags = 0;

	if (win32::WSARecv(
		socket,
		// This function is not const correct, so a const_cast is required.
		const_cast<WSABUF*>(wsa_buffers.data()),
		vsm::saturating(wsa_buffers.size()),
		&transferred,
		&flags,
		overlapped,
		/* lpCompletionRoutine: */ nullptr) == SOCKET_ERROR)
	{
		if (int const e = WSAGetLastError(); e != WSA_IO_PENDING)
		{
			return vsm::unexpected(allio_error(static_cast<posix::socket_error>(e)));
		}

		vsm_try_void(overlapped.wait(
			socket,
			a.deadline,
			&transferred,
			&flags));
	}

	if (transferred == 0 && !io_buffers_is_empty(a.buffers))
	{
		return vsm::unexpected(allio_error(error::end_of_stream));
	}

	return transferred;
}

vsm::result<size_t> raw_socket_t::stream_write(
	native_handle<raw_socket_t> const& h,
	io_parameters_t<raw_socket_t, stream_write_t> const& a)
{
	if (a.buffers.get_buffers_size() > max_buffer_count)
	{
		return vsm::unexpected(allio_error(error::too_many_io_buffers));
	}

	if (a.deadline != deadline::never() &&
		h.flags[platform_object_t::impl_type::flags::synchronous])
	{
		return vsm::unexpected(allio_error(error::unsupported_operation));
	}

	SOCKET const socket = posix::unwrap_socket(h.platform_handle);

	automatic_wsa_buffer_storage buffer_storage;
	vsm_try(wsa_buffers, get_wsa_buffers(a.buffers, buffer_storage));

	vsm_try(overlapped, wsa_thread_overlapped::get_for(h));

	DWORD transferred;
	DWORD flags = 0;

	if (win32::WSASend(
		socket,
		// This function is not const correct, so a const_cast is required.
		const_cast<WSABUF*>(wsa_buffers.data()),
		vsm::saturating(wsa_buffers.size()),
		&transferred,
		flags,
		overlapped,
		/* lpCompletionRoutine: */ nullptr) == SOCKET_ERROR)
	{
		if (int const e = WSAGetLastError(); e != WSA_IO_PENDING)
		{
			return vsm::unexpected(allio_error(static_cast<posix::socket_error>(e)));
		}

		vsm_try_void(overlapped.wait(
			socket,
			a.deadline,
			&transferred,
			&flags));
	}

	return transferred;
}

vsm::result<void> raw_socket_t::close(
	native_handle<raw_socket_t>& h,
	io_parameters_t<raw_socket_t, close_t> const& a)
{
	native_platform_handle const handle = h.platform_handle;
	if (handle != native_platform_handle::null)
	{
		h.platform_handle = native_platform_handle::null;
		posix::close_socket(posix::unwrap_socket(handle));
	}

	h.flags = handle_flags::none;

	return {};
}
