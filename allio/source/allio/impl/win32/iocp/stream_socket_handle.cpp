#include <allio/win32/detail/iocp/stream_socket_handle.hpp>

#include <allio/impl/win32/iocp/socket.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/impl/win32/wsa_ex.hpp>
#include <allio/posix/socket.hpp>
#include <allio/win32/kernel_error.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

using M = iocp_multiplexer;
using H = _stream_socket_handle;
using C = connector_t<M, H>;

using connect_t = _stream_socket_handle::connect_t;
using connect_s = operation_t<M, H, connect_t>;
using connect_r = io_result_ref_t<connect_t>;

io_result operation_impl<M, H, connect_t>::submit(M& m, H const& h, C const& c, connect_s& s, connect_r)
{
	if (!h)
	{
		return error::handle_is_null;
	}

	if (h.get_flags()[H::flags::bound])
	{
		return error::socket_already_bound;
	}

	SOCKET const socket = unwrap_socket(h.get_platform_handle());
	vsm_try(addr, socket_address::make(s.args.endpoint));

	// The socket must be bound before calling ConnectEx.
	{
		socket_address_union bind_addr;
		memset(&bind_addr, 0, sizeof(bind_addr));
		bind_addr.addr.sa_family = addr.addr.sa_family;

		if (::bind(socket, &bind_addr.addr, addr.size) == SOCKET_ERROR)
		{
			return get_last_socket_error();
		}
	}

	OVERLAPPED& overlapped = *s.overlapped;
	overlapped.Pointer = nullptr;
	overlapped.hEvent = NULL;

	DWORD const error = wsa_connect_ex(
		socket,
		addr,
		overlapped);

	if (error == ERROR_IO_PENDING)
	{
		return std::nullopt;
	}

	if (error != 0)
	{
		return static_cast<socket_error>(error);
	}

	if (!m.supports_synchronous_completion(h))
	{
		return std::nullopt;
	}

	return std::error_code();
}

io_result operation_impl<M, H, connect_t>::notify(M& m, H const& h, C const& c, connect_s& s, connect_r const r, io_status const p_status)
{
	auto const& status = M::unwrap_io_status(p_status);
	vsm_assert(&status.slot == &s.overlapped);
	return get_kernel_error_code(status.status);
}

void operation_impl<M, H, connect_t>::cancel(M& m, H const& h, C const& c, S& s)
{
	cancel_socket_io(unwrap_socket(h.get_platform_handle()), *s.overlapped);
}
