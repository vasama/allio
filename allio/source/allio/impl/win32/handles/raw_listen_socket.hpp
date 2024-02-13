#include <allio/detail/handles/raw_listen_socket.hpp>

#include <allio/impl/posix/socket.hpp>

#include <vsm/lazy.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

using accept_result_type = accept_result<basic_detached_handle<raw_socket_t>>;

vsm::result<void> raw_listen_socket_t::listen(
	native_handle<raw_listen_socket_t>& h,
	io_parameters_t<raw_listen_socket_t, listen_t> const& a)
{
	vsm_try(addr, socket_address::make(a.endpoint));
	vsm_try(protocol, choose_protocol(addr.addr.sa_family, SOCK_STREAM));

	vsm_try_bind((socket, flags), create_socket(
		addr.addr.sa_family,
		SOCK_STREAM,
		protocol,
		a.flags));

	vsm_try_void(socket_listen(
		socket.get(),
		addr,
		a.backlog));

	h.flags = flags::not_null | flags;
	h.platform_handle = wrap_socket(socket.release());

	return {};
}

vsm::result<accept_result_type> raw_listen_socket_t::accept(
	native_handle<raw_listen_socket_t> const& h,
	io_parameters_t<raw_listen_socket_t, accept_t> const& a)
{
	wsa_accept_address_buffer wsa_addr;
	posix::socket_address_union& addr = wsa_addr.remote;

	vsm_try_bind((socket, flags), socket_accept(
		unwrap_socket(h.platform_handle),
		addr,
		a.deadline,
		a.flags));

	if (vsm::any_flags(a.flags, io_flags::create_non_blocking) || a.deadline != deadline::never())
	{
		vsm_try(overlapped, wsa_thread_overlapped::get());

		;

		DWORD transferred = static_cast<DWORD>(-1);
		if (!win32::AcceptEx(
			unwrap_socket(h.platform_handle),
			//TODO
			NULL,
			/* lpOutputBuffer: */ &wsa_addr,
			/* dwReceiveDataLength: */ 0,
			sizeof(wsa_addr.local),
			sizeof(wsa_addr.remote),
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
		int addr_size = sizeof(posix::socket_address_union);
		
		SOCKET const socket = win32::WSAAccept(
			unwrap_handle(h.platform_handle),
			&addr.addr,
			&addr_size,
			/* lpfnCondition: */ nullptr,
			/* dwCallbackData: */ 0);

		if (socket == SOCKET_ERROR)
		{
			return vsm::unexpected(posix::get_last_socket_error());
		}
	}

	auto const make_socket_handle = [&]()
	{
		native_handle<raw_socket_t> h = {};
		h.flags = object_t::flags::not_null | flags;
		h.platform_handle = wrap_socket(socket.release());
		return basic_detached_handle<raw_socket_t>(adopt_handle, h);
	};

	return vsm::result<accept_result_type>(
		vsm::result_value,
		vsm_lazy(make_socket_handle()),
		addr.get_network_endpoint());
}

vsm::result<void> raw_listen_socket_t::close(
	native_handle<raw_listen_socket_t>& h,
	io_parameters_t<raw_listen_socket_t, close_t> const&)
{
	posix::close_socket(unwrap_socket(h.platform_handle));
	h = {};
	return {};
}
