#include <allio/win32/detail/iocp/raw_socket.hpp>

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
using H = native_handle<raw_socket_t>;
using C = async_connector_t<M, raw_socket_t>;

using connect_t = raw_socket_t::connect_t;
using connect_s = async_operation_t<M, raw_socket_t, connect_t>;
using connect_a = io_parameters_t<raw_socket_t, connect_t>;

io_result<void> connect_s::submit(M& m, H& h, C& c, connect_s& s, connect_a const& a, io_handler<M>& handler)
{
	vsm_try(addr, posix::socket_address::make(a.endpoint));
	vsm_try(protocol, posix::choose_protocol(addr.addr.sa_family, SOCK_STREAM));

	unique_handle handle;
	{
		NTSTATUS const status = AfdCreateSocket(
			vsm::out_resource(handle),
			/* EndpointFlags: */ 0,
			/* GroupID: */ NULL,
			static_cast<ULONG>(addr.sa_family),
			static_cast<ULONG>(SOCK_STREAM),
			static_cast<ULONG>(protocol),
			/* CreateOptions: */ 0,
			/* TransportName: */ nullptr);
	
		if (!NT_SUCCESS(status))
		{
			vsm::unexpected(static_cast<kernel_error>(status));
		}
	}

	// The socket must be bound before connecting.
	{
		posix::socket_address_union bind_addr;
		memset(&bind_addr, 0, sizeof(bind_addr));
		bind_addr.addr.sa_family = addr.addr.sa_family;

		NTSTATUS const status = AfdBind(
			handle.get(),
			AFD_SHARE_WILDCARD,
			&bind_addr.addr
			addr.size);
		vsm_assert(status != STATUS_PENDING);

		if (!NT_SUCCESS(status))
		{
			return vsm::unexpected(static_cast<kernel_error>(status));
		}
	}

	s.io_status_block.bind(handler);

	bool const may_complete_synchronously = m.supports_synchronous_completion(h);

	// If using a multithreaded completion port, after this call
	// another thread will race to complete this operation.
	NTSTATUS const status = AfdConnect(
		handle.get(),
		/* Event: */ NULL,
		/* ApcRoutine: */ nullptr,
		/* ApcContext: */ nullptr,
		s.io_status_block.get(),
		&addr.addr,
		static_cast<ULONG>(addr.size));

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	if (may_complete_synchronously && status != STATUS_PENDING)
	{
		h.flags = object_t::flags::not_null | s.socket_flags;
		h.platform_handle = s.socket.release();

		return {};
	}

	return io_pending(error::async_operation_pending);
}

io_result<void> connect_s::notify(M&, H& h, C&, connect_s& s, connect_a const&, M::io_status_type const status)
{
	vsm_assert(&status.slot == &s.io_status_block);

	if (!NT_SUCCESS(status.status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status.status));
	}

	h.flags = object_t::flags::not_null | s.socket_flags;
	h.platform_handle = s.socket.release();

	return {};
}

void connect_s::cancel(M&, H const&, C const&, S& s)
{
	cancel_io(s.handle.get(), *s.io_status_block);
}


using read_t = raw_socket_t::stream_read_t;
using read_s = async_operation_t<M, raw_socket_t, read_t>;
using read_a = io_parameters_t<raw_socket_t, read_t>;

io_result<size_t> read_s::submit(M& m, H const& h, C const&, read_s& s, read_a const& a, io_handler<M>& handler)
{
	vsm_try(wsa_buffers, make_wsa_buffers(s.buffers, a.buffers.buffers()));

	s.io_status_block.bind(handler);

	bool const may_complete_synchronously = m.supports_synchronous_completion(h);

	AFD_RECV_INFO info =
	{
		.Buffers = wsa_buffers.data,
		.BufferCount = wsa_buffers.size,
	};

	// If using a multithreaded completion port, after this call
	// another thread will race to complete this operation.
	NTSTATUS const status = AfdRecv(
		unwrap_handle(h.platform_handle),
		/* Event: */ NULL,
		/* ApcRoutine: */ nullptr,
		/* ApcContext: */ nullptr,
		s.io_status_block.get(),
		&info);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	if (may_complete_synchronously && status != STATUS_PENDING)
	{
		return io_status_block.Information;
	}

	return io_pending(error::async_operation_pending);
}

io_result<size_t> read_s::notify(M&, H const& h, C const&, read_s& s, read_a const&, M::io_status_type const status)
{
	vsm_assert(&status.slot == &s.io_status_block);

	if (!NT_SUCCESS(status.status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status.status));
	}

	return s.io_status_block->Information;
}

void read_s::cancel(M&, H const& h, C const&, read_s& s)
{
	cancel_io(s.handle.get(), *s.io_status_block);
}


using write_t = raw_socket_t::stream_write_t;
using write_s = async_operation_t<M, raw_socket_t, write_t>;
using write_a = io_parameters_t<raw_socket_t, write_t>;

io_result<size_t> write_s::submit(M& m, H const& h, C const&, write_s& s, write_a const& a, io_handler<M>& handler)
{
	vsm_try(wsa_buffers, make_wsa_buffers(s.buffers, a.buffers.buffers()));

	s.io_status_block.bind(handler);

	bool const may_complete_synchronously = m.supports_synchronous_completion(h);

	AFD_SEND_INFO info =
	{
		.Buffers = wsa_buffers.data,
		.BufferCount = wsa_buffers.size,
	};

	// If using a multithreaded completion port, after this call
	// another thread will race to complete this operation.
	NTSTATUS const status = AfdSend(
		unwrap_handle(h.platform_handle),
		/* Event: */ NULL,
		/* ApcRoutine: */ nullptr,
		/* ApcContext: */ nullptr,
		s.io_status_block.get(),
		&info);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	if (may_complete_synchronously && status != STATUS_PENDING)
	{
		return io_status_block.Information;
	}

	return io_pending(error::async_operation_pending);
}

io_result<size_t> write_s::notify(M&, H const& h, C const&, write_s& s, write_a const&, M::io_status_type const status)
{
	vsm_assert(&status.slot == &s.overlapped);

	if (!NT_SUCCESS(status.status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status.status));
	}

	return s.io_status_block->Information;
}

void write_s::cancel(M&, H const& h, C const&, write_s& s)
{
	cancel_io(s.handle.get(), *s.io_status_block);
}
