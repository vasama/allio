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

struct accept_data
{
	AFD_RECEIVED_ACCEPT_DATA data;
	posix::socket_address_union addr;
};

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

static io_result<accept_result_type> continue_accept_2(M& m, H const& h, accept_s& s)
{
	auto& data = new_object<accept_data>(s.storage);

	unique_handle& handle = s.handle;
	socket_handle_type::connector_type new_c;
	vsm_try_void(m.attach_handle(handle.get(), new_c));

	auto const make_socket_handle = [&]()
	{
		native_handle<raw_socket_t> new_h = {};
		h.flags = object_t::flags::not_null | h_flags;
		h.platform_handle = handle.release();
		return socket_handle_type(adopt_handle, m, new_h, new_c);
	};

	return vsm_lazy(accept_result_type
	{
		make_socket_handle(),
		addr.get_network_endpoint(),
	});
}

static io_result<accept_result_type> continue_accept_1(M& m, H const& h, accept_s& s)
{
	auto const& data = get_object<accept_data>(s.storage);

	// Create the accept socket
	{
		vsm_assert(!s.handle);

		NTSTATUS const status = AfdCreateSocket(
			vsm::out_resource(s.handle),
			/* EndpointFlags: */ 0,
			/* GroupID: */ NULL,
			static_cast<ULONG>(addr.sa_family),
			static_cast<ULONG>(SOCK_STREAM),
			static_cast<ULONG>(protocol),
			create_options,
			/* TransportName: */ nullptr);
	
		if (!NT_SUCCESS(status))
		{
			return vsm::unexpected(static_cast<kernel_error>(status));
		}
	}

	bool const may_complete_synchronously = m.supports_synchronous_completion(h);

	// If using a multithreaded completion port, after this call
	// another thread will race to complete this operation.
	NTSTATUS const status = AfdAccept(
		unwrap_handle(h.platform_handle),
		/* Event: */ NULL,
		/* ApcRoutine: */ nullptr,
		/* ApcContext: */ nullptr,
		s.io_status_block.get(),
		s.handle.get(),
		data.data.SequenceNumber);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	if (may_complete_synchronously && status != STATUS_PENDING)
	{
		return continue_accept_2(m, h, s);
	}

	return io_pending(error::async_operation_pending);
}

io_result<accept_result_type> accept_s::submit(M& m, H const& h, C const&, accept_s& s, accept_a const& a, io_handler<M>& handler)
{
	auto& data = new_object<accept_data>(s.storage);

	s.io_status_block.bind(handler);

	bool const may_complete_synchronously = m.supports_synchronous_completion(h);

	// If using a multithreaded completion port, after this call
	// another thread will race to complete this operation.
	NTSTATUS const status = AfdWaitForAccept(
		unwrap_handle(h.platform_handle),
		/* Event: */ NULL,
		/* ApcRoutine: */ nullptr,
		/* ApcContext: */ nullptr,
		s.io_status_block.get(),
		&data.data,
		sizeof(data));

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	if (may_complete_synchronously && status != STATUS_PENDING)
	{
		return continue_accept_1(m, h, s);
	}

	return io_pending(error::async_operation_pending);
}

io_result<accept_result_type> accept_s::notify(M& m, H const&, C const&, accept_s& s, accept_a const&, M::io_status_type const status)
{
	vsm_assert(&status.slot == &s.io_status_block);

	if (!NT_SUCCESS(status.status))
	{
		s.handle.reset();
		return vsm::unexpected(static_cast<kernel_error>(status.status));
	}

	if (s.handle)
	{
		return continue_accept_2(m, h, s);
	}
	else
	{
		return continue_accept_1(m, h, s);
	}
}

void accept_s::cancel(M&, H const& h, C const&, S& s)
{
	cancel_io(unwrap_handler(h.platform_handle), *s.io_status_block);
}
