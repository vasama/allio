#include <allio/win32/detail/iocp/raw_listen_socket.hpp>

#include <allio/impl/posix/socket.hpp>
#include <allio/impl/win32/iocp/raw_socket.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/impl/win32/wsa.hpp>
#include <allio/win32/kernel_error.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

using M = iocp_multiplexer;
using H = native_handle<raw_listen_socket_t>;
using C = async_connector_t<M, raw_listen_socket_t>;

using socket_handle_type = basic_attached_handle<raw_socket_t, basic_multiplexer_handle<M>>;
using accept_result_type = accept_result<socket_handle_type>;


using listen_t = raw_listen_socket_t::listen_t;
using listen_s = async_operation_t<M, raw_listen_socket_t, listen_t>;
using listen_a = io_parameters_t<raw_listen_socket_t, listen_t>;

io_result<void> listen_s::submit(M& m, H& h, C& c, listen_s&, listen_a const& a, io_handler<M>&)
{
	vsm_try(addr, posix::socket_address::make(a.endpoint));
	vsm_try(protocol, posix::choose_protocol(addr.addr.sa_family, SOCK_STREAM));

	vsm_try_bind((socket, flags), posix::create_socket(
		addr.addr.sa_family,
		SOCK_STREAM,
		protocol,
		a.flags | io_flags::create_non_blocking));

	vsm_try_void(posix::socket_listen(
		socket.get(),
		addr,
		a.backlog));

	vsm_try_void(m.attach_handle(
		posix::wrap_socket(socket.get()),
		c));

	h.flags = object_t::flags::not_null | flags;
	h.platform_handle = posix::wrap_socket(socket.release());

	return {};
}

io_result<void> listen_s::notify(M&, H&, C&, listen_s&, listen_a const&, M::io_status_type)
{
	vsm_unreachable();
}

void listen_s::cancel(M&, H const&, C const&, listen_s&)
{
}


using accept_t = raw_listen_socket_t::accept_t;
using accept_s = async_operation_t<M, raw_listen_socket_t, accept_t>;
using accept_a = io_parameters_t<raw_listen_socket_t, accept_t>;

static io_result<accept_result_type> make_accept_result(
	M& m,
	unique_wrapped_socket socket,
	handle_flags const flags,
	posix::socket_address_union const& addr)
{
	socket_handle_type::connector_type c;
	vsm_try_void(m.attach_handle(socket.get(), c));

	auto const make_socket_handle = [&]()
	{
		native_handle<raw_socket_t> h = {};
		h.flags = object_t::flags::not_null | flags;
		h.platform_handle = socket.release();
		return socket_handle_type(adopt_handle, m, h, c);
	};

	return vsm_lazy(accept_result_type
	{
		make_socket_handle(),
		addr.get_network_endpoint(),
	});
}

io_result<accept_result_type> accept_s::submit(M& m, H const& h, C const&, accept_s& s, accept_a const& a, io_handler<M>& handler)
{
	SOCKET const listen_socket = posix::unwrap_socket(h.platform_handle);
	//TODO: Cache the address family.
	vsm_try(addr, posix::socket_address::get(listen_socket));
	vsm_try(protocol, posix::choose_protocol(addr.addr.sa_family, SOCK_STREAM));

	vsm_try_bind((socket, flags), posix::create_socket(
		addr.addr.sa_family,
		SOCK_STREAM,
		protocol,
		a.flags | io_flags::create_non_blocking));

	s.socket = unique_wrapped_socket(posix::wrap_socket(socket.release()));
	s.socket_flags = flags;

	OVERLAPPED& overlapped = *s.overlapped;
	overlapped.Pointer = nullptr;
	overlapped.hEvent = NULL;

	auto& addr_buffer = new_wsa_address_buffer<wsa_accept_address_buffer>(s.address_storage);

	s.overlapped.bind(handler);

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

	return io_pending(error::async_operation_pending);
}

io_result<accept_result_type> accept_s::notify(M& m, H const&, C const&, accept_s& s, accept_a const&, M::io_status_type const status)
{
	vsm_assert(&status.slot == &s.overlapped);

	if (!NT_SUCCESS(status.status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status.status));
	}

	auto& addr_buffer = get_wsa_address_buffer<wsa_accept_address_buffer>(s.address_storage);
	return make_accept_result(m, vsm_move(s.socket), s.socket_flags, addr_buffer.remote);
}

void accept_s::cancel(M&, H const& h, C const&, S& s)
{
	cancel_socket_io(posix::unwrap_socket(h.platform_handle), *s.overlapped);
}
