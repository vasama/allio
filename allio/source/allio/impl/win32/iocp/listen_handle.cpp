#include <allio/win32/detail/iocp/listen_handle.hpp>

#include <allio/impl/posix/socket.hpp>
#include <allio/impl/win32/iocp/socket.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/impl/win32/wsa_ex.hpp>
#include <allio/win32/kernel_error.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

using M = iocp_multiplexer;
using H = raw_listen_handle_t;
using N = H::native_type;
using C = connector_t<M, H>;

using socket_handle_type = async_handle<raw_socket_handle_t, basic_multiplexer_handle<M>>;
using accept_result_type = accept_result<socket_handle_type>;

using listen_t = H::listen_t;
using listen_i = operation_impl<M, H, listen_t>;
using listen_s = operation_t<M, H, listen_t>;

io_result2<void> listen_i::submit(M& m, N& h, C& c, listen_s& s)
{
	vsm_try(addr, posix::socket_address::make(s.args.endpoint));

	vsm_try(socket, posix::create_socket(
		addr.addr.sa_family,
		/* multiplexable: */ true));

	vsm_try_void(posix::socket_listen(
		socket.socket.get(),
		addr,
		s.args.backlog));

	vsm_try_void(m.attach_handle(
		posix::wrap_socket(socket.socket.get()),
		c));

	h = platform_object_t::native_type
	{
		{
			H::flags::not_null | socket.flags,
		},
		posix::wrap_socket(socket.socket.release()),
	};

	return {};
}

io_result2<void> listen_i::notify(M& m, N& h, C& c, listen_s& s, io_status const p_status)
{
	vsm_unreachable();
}

void listen_i::cancel(M& m, N const& h, C const& c, listen_s& s)
{
}

using accept_t = H::accept_t;
using accept_i = operation_impl<M, H, accept_t>;
using accept_s = operation_t<M, H, accept_t>;

static io_result2<accept_result_type> make_accept_result(
	M& m,
	unique_wrapped_socket socket,
	handle_flags const flags,
	posix::socket_address_union const& addr)
{
	socket_handle_type::connector_type c;

	vsm_try_void(m.attach_handle(socket.get(), c));

	return vsm_lazy(accept_result_type
	{
		socket_handle_type
		(
			adopt_handle_t(),
			platform_object_t::native_type
			{
				{
					H::flags::not_null | flags,
				},
				socket.release(),
			},
			m,
			vsm_move(c)
		),
		addr.get_network_endpoint(),
	});
}

io_result2<accept_result_type> accept_i::submit(M& m, N const& h, C const& c, accept_s& s)
{
	SOCKET const listen_socket = posix::unwrap_socket(h.platform_handle);
	//TODO: Cache the address family.
	vsm_try(addr, posix::socket_address::get(listen_socket));

	vsm_try(socket, posix::create_socket(
		addr.addr.sa_family,
		/* multiplexable: */ true));

	s.socket = unique_wrapped_socket(posix::wrap_socket(socket.socket.release()));
	s.socket_flags = socket.flags;

	OVERLAPPED& overlapped = *s.overlapped;
	overlapped.Pointer = nullptr;
	overlapped.hEvent = NULL;

	static_assert(sizeof(s.address_storage) >= sizeof(wsa_accept_address_buffer));
	auto& addr_buffer = *new (s.address_storage) wsa_accept_address_buffer;

	s.overlapped.bind(s);

	// If using a multithreaded completion port, after this call
	// another thread will race to complete this operation.
	vsm_try(already_connected, submit_socket_io(m, h, [&]()
	{
		return wsa_accept_ex(
			listen_socket,
			posix::unwrap_socket(s.socket.get()),
			addr_buffer,
			overlapped);
	}));

	if (already_connected)
	{
		return make_accept_result(m, vsm_move(s.socket), s.socket_flags, addr_buffer.remote);
	}

	return io_pending;
}

io_result2<accept_result_type> accept_i::notify(M& m, N const& h, C const& c, accept_s& s, io_status const p_status)
{
	auto const& status = M::unwrap_io_status(p_status);
	vsm_assert(&status.slot == &s.overlapped);

	if (!NT_SUCCESS(status.status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status.status));
	}

	auto& addr_buffer = *std::launder(reinterpret_cast<wsa_accept_address_buffer*>(s.address_storage));
	return make_accept_result(m, vsm_move(s.socket), s.socket_flags, addr_buffer.remote);
}

void accept_i::cancel(M& m, N const& h, C const& c, S& s)
{
	cancel_socket_io(posix::unwrap_socket(h.platform_handle), *s.overlapped);
}
