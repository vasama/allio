#include <allio/win32/detail/iocp/listen_socket_handle.hpp>

#include <allio/impl/posix/socket.hpp>
#include <allio/impl/win32/iocp/socket.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/impl/win32/wsa_ex.hpp>
#include <allio/win32/kernel_error.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::posix;
using namespace allio::win32;

using M = iocp_multiplexer;
using H = _listen_socket_handle;
using C = connector_t<M, H>;

using accept_t = _listen_socket_handle::accept_t;
using accept_s = operation_t<M, H, accept_t>;
using accept_r = io_result_ref_t<accept_t>;

static io_result handle_completion(accept_r const r, unique_socket socket, posix::socket_address_union const& addr)
{
	//TODO: Set file completion notification modes

	vsm_verify(r.result.socket.set_native_handle(
	{
		{
			H::flags::not_null,
		},
		wrap_socket(socket.get()),
	}));
	(void)socket.release();

	r.result.endpoint = addr.get_network_endpoint();

	return std::error_code();
}

io_result operation_impl<M, H, accept_t>::submit(M& m, H const& h, C const& c, accept_s& s, accept_r const r)
{
	if (!h)
	{
		return error::handle_is_null;
	}

	SOCKET const socket_listen = unwrap_socket(h.get_platform_handle());
	//TODO: Cache the address family.
	vsm_try(addr, socket_address::get(socket_listen));
	vsm_try(socket, create_socket(addr.addr.sa_family));
	s.socket = vsm_move(socket.socket);

	OVERLAPPED& overlapped = *s.overlapped;
	overlapped.Pointer = nullptr;
	overlapped.hEvent = NULL;

	static_assert(sizeof(s.address_storage) >= sizeof(wsa_accept_address_buffer));
	auto& addr_buffer = *new (s.address_storage) wsa_accept_address_buffer;

	DWORD const error = wsa_accept_ex(
		socket_listen,
		s.socket.get(),
		addr_buffer,
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

	return handle_completion(r, vsm_move(s.socket), addr_buffer.remote);
}

io_result operation_impl<M, H, accept_t>::notify(M& m, H const& h, C const& c, accept_s& s, accept_r const r, io_status const p_status)
{
	auto const& status = M::unwrap_io_status(p_status);
	vsm_assert(&status.slot == &s.overlapped);

	if (!NT_SUCCESS(status.status))
	{
		return static_cast<kernel_error>(status.status);
	}

	auto& addr_buffer = *std::launder(reinterpret_cast<wsa_accept_address_buffer*>(s.address_storage));
	return handle_completion(r, vsm_move(s.socket), addr_buffer.remote);
}

void operation_impl<M, H, accept_t>::cancel(M& m, H const& h, C const& c, S& s)
{
	cancel_socket_io(unwrap_socket(h.get_platform_handle()), *s.overlapped);
}
