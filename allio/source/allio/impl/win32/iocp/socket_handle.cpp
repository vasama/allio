#include <allio/win32/detail/iocp/socket_handle.hpp>

#include <allio/impl/posix/socket.hpp>
#include <allio/impl/win32/iocp/socket.hpp>
#include <allio/impl/win32/kernel.hpp>
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
using connect_s = operation_t<M, H, connect_t>;

io_result2<void> operation_impl<M, H, connect_t>::submit(M& m, N& h, C const& c, connect_s& s)
{
	vsm_try(addr, posix::socket_address::make(s.args.endpoint));
	vsm_try(socket, posix::create_socket(addr.addr.sa_family));

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

	OVERLAPPED& overlapped = *s.overlapped;
	overlapped.Pointer = nullptr;
	overlapped.hEvent = NULL;

	// If using a multithreaded completion port, after this call
	// another thread will race to complete this operation.
	vsm_try(already_connected, submit_socket_io(
		m,
		h,
		[&]()
		{
			return wsa_connect_ex(
				posix::unwrap_socket(s.socket.get()),
				addr,
				overlapped);
		}));

	if (already_connected)
	{
		return {};
	}

	return io_pending;
}

io_result2<void> operation_impl<M, H, connect_t>::notify(M& m, N& h, C const& c, connect_s& s, io_status const p_status)
{
	auto const& status = M::unwrap_io_status(p_status);
	vsm_assert(&status.slot == &s.overlapped);

	if (!NT_SUCCESS(status.status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status.status));
	}

	return {};
}

void operation_impl<M, H, connect_t>::cancel(M& m, N const& h, C const& c, S& s)
{
	cancel_socket_io(posix::unwrap_socket(s.socket.get()), *s.overlapped);
}
