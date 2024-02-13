#include <allio/detail/handles/raw_socket.hpp>

#include <allio/impl/posix/socket.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

namespace {

class wsa_thread_overlapped
{
	thread_event m_event;
	OVERLAPPED m_overlapped;

public:
	static [[nodiscard]] vsm::result<wsa_thread_overlapped> get_for(
		native_handle<platform_object_t> const& h)
	{
		vsm::result<wsa_thread_overlapped> r(vsm::result_value);
		if (!h.flags[platform_object_t::impl_type::flags::synchronous])
		{
			if (auto r2 = thread_event::get())
			{
				r->m_event = vsm_move(*r2);
			}
			else
			{
				r = vsm::unexpected(r2.error());
			}
		}
		return r;
	}

	[[nodiscard]] vsm::result<void> wait(
		SOCKET const socket,
		deadline const deadline,
		DWORD* const transferred,
		DWORD* const flags)
	{
		(void)m_event.wait_for_io(
			(HANDLE)socket,
			m_overlapped,
			deadline);

		if (!WSAGetOverlappedResult(
			socket,
			&m_overlapped,
			transferred,
			FALSE,
			flags))
		{
			return vsm::unexpected(get_last_socket_error());
		}

		return {};
	}

	template<std::same_as<OVERLAPPED> Overlapped>
	[[nodiscard]] operator Overlapped*() &
	{
		if (m_event)
		{
			m_overlapped.Pointer = nullptr;
			m_overlapped.hEvent = m_event;
			return &m_overlapped;
		}

		return nullptr;
	}
};

} // namespace

vsm::result<void> raw_socket_t::connect(
	native_handle<raw_socket_t>& h,
	io_parameters_t<raw_socket_t, connect_t> const& a)
{
	vsm_try(addr, posix::socket_address::make(a.endpoint));
	vsm_try(protocol, posix::choose_protocol(addr.addr.sa_family, SOCK_STREAM));

	vsm_try_bind((socket, flags), posix::create_socket(
		addr.addr.sa_family,
		SOCK_STREAM,
		protocol,
		a.flags));

	if (vsm::any_flags(a.flags, io_flags::create_non_blocking) || a.deadline != deadline::never())
	{
		vsm_try(overlapped, wsa_thread_overlapped::get());

		DWORD transferred = static_cast<DWORD>(-1);
		if (!win32::ConnectEx(
			socket.get(),
			&addr.addr,
			addr.size,
			/* lpSendBuffer: */ nullptr,
			/* dwSendDataLength: */ 0,
			&transferred,
			overlapped))
		{
			if (int const e = WSAGetLastError(); e != WSA_IO_PENDING)
			{
				return vsm::unexpected(static_cast<socket_error>(e));
			}

			DWORD flags;
			vsm_try_void(overlapped.wait(
				socket,
				a.deadline,
				&transferred,
				&flags));
		}
		vsm_assert(transferred == 0);
	}
	else
	{
		if (::connect(socket, &addr.addr, addr.size) == SOCKET_ERROR)
		{
			return vsm::unexpected(get_last_socket_error());
		}
	}

	h.flags = flags::not_null | flags;
	h.platform_handle = wrap_socket(socket.release());

	return {};
}

vsm::result<size_t> raw_socket_t::stream_read(
	native_handle<raw_socket_t> const& h,
	io_parameters_t<raw_socket_t, stream_read_t> const& a)
{
	wsa_buffers_storage<64> buffers_storage;
	vsm_try(wsa_buffers, make_wsa_buffers(buffers_storage, a.buffers.buffers()));

	//TODO: Handle finite timeouts.
	vsm_try(overlapped, wsa_thread_overlapped::get_for(h));

	DWORD transferred;
	DWORD flags = 0;

	if (win32::WSARecv(
		socket,
		wsa_buffers.data,
		wsa_buffers.size,
		&transferred,
		&flags,
		overlapped,
		/* lpCompletionRoutine: */ nullptr) == SOCKET_ERROR)
	{
		if (int const e = WSAGetLastError(); e != WSA_IO_PENDING)
		{
			return vsm::unexpected(static_cast<socket_error>(e));
		}

		vsm_try_void(overlapped.wait(
			socket,
			a.deadline,
			&transferred,
			&flags));
	}

	return transferred;
}

vsm::result<size_t> raw_socket_t::stream_write(
	native_handle<raw_socket_t> const& h,
	io_parameters_t<raw_socket_t, stream_write_t> const& a)
{
	wsa_buffers_storage<64> buffers_storage;
	vsm_try(wsa_buffers, make_wsa_buffers(buffers_storage, a.buffers.buffers()));

	//TODO: Handle finite timeouts.
	vsm_try(overlapped, wsa_thread_overlapped::get_for(h));

	DWORD transferred;
	DWORD flags = 0;

	if (win32::WSASend(
		socket,
		wsa_buffers.data,
		wsa_buffers.size,
		&transferred,
		flags,
		overlapped,
		/* lpCompletionRoutine: */ nullptr) == SOCKET_ERROR)
	{
		if (int const e = WSAGetLastError(); e != WSA_IO_PENDING)
		{
			return vsm::unexpected(static_cast<socket_error>(e));
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
	io_parameters_t<raw_socket_t, close_t> const&)
{
	posix::close_socket(unwrap_socket(h.platform_handle));
	h = {};
	return {};
}
