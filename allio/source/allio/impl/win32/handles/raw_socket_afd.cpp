#include <allio/detail/handles/raw_socket.hpp>

#include <allio/impl/posix/socket.hpp>
#include <allio/impl/win32/afd.hpp>

#include <vsm/out_resource.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

vsm::result<void> raw_socket_t::connect(
	native_handle<raw_socket_t>& h,
	io_parameters_t<raw_socket_t, connect_t> const& a)
{
	vsm_try(addr, posix::socket_address::make(a.endpoint));
	vsm_try(protocol, posix::choose_protocol(addr.addr.sa_family, SOCK_STREAM));

	ULONG create_options = 0;
	if (vsm::any_flags(a.flags, io_flags::create_non_blocking))
	{
		create_options |= FILE_SYNCHRONOUS_IO_NONALERT;
	}

	unique_handle handle;
	{
		NTSTATUS const status = AfdCreateSocket(
			vsm::out_resource(handle),
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

	// Connect
	{
		IO_STATUS_BLOCK io_status_block;

		NTSTATUS status = AfdConnect(
			handle.get(),
			/* Event: */ NULL,
			/* ApcRoutine: */ nullptr,
			/* ApcContext: */ nullptr,
			&io_status_block,
			&addr.addr,
			static_cast<ULONG>(addr.size));
	
		if (status == STATUS_PENDING)
		{
			status = io_wait(
				handle.get(),
				io_status_block,
				a.deadline);
		}
	
		if (!NT_SUCCESS(status))
		{
			vsm::unexpected(static_cast<kernel_error>(status));
		}
	}

	auto h_flags = handle_flags::none;
	if (vsm::any_flags(create_options, FILE_SYNCHRONOUS_IO_NONALERT))
	{
		h_flags |= impl_type::flags::synchronous;
	}
	else
	{
		h_flags |= set_file_completion_notification_modes(handle.get());
	}

	h.flags = flags::not_null | h_flags;
	h.platform_handle = wrap_handle(handle.release());

	return {};
}

vsm::result<size_t> raw_socket_t::stream_read(
	native_handle<raw_socket_t> const& h,
	io_parameters_t<raw_socket_t, stream_read_t> const& a)
{
	wsa_buffers_storage<64> buffers_storage;
	vsm_try(wsa_buffers, make_wsa_buffers(buffers_storage, a.buffers.buffers()));

	vsm_try(event, thread_event::get_for(h));

	AFD_RECV_INFO info =
	{
		.Buffers = wsa_buffers.data,
		.BufferCount = wsa_buffers.size,
	};

	IO_STATUS_BLOCK io_status_block;

	NTSTATUS status = AfdRecv(
		unwrap_handle(h.platform_handle),
		event,
		/* ApcRoutine: */ nullptr,
		/* ApcContext: */ nullptr,
		&io_status_block,
		&info);

	if (status == STATUS_PENDING)
	{
		status = event.wait_for_io(
			unwrap_handle(h.platform_handle),
			io_status_block,
			a.deadline);
	}

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return io_status_block.Information;
}

vsm::result<size_t> raw_socket_t::stream_write(
	native_handle<raw_socket_t> const& h,
	io_parameters_t<raw_socket_t, stream_write_t> const& a)
{
	wsa_buffers_storage<64> buffers_storage;
	vsm_try(wsa_buffers, make_wsa_buffers(buffers_storage, a.buffers.buffers()));

	vsm_try(event, thread_event::get_for(h));

	AFD_SEND_INFO info =
	{
		.Buffers = wsa_buffers.data,
		.BufferCount = wsa_buffers.size,
	};

	IO_STATUS_BLOCK io_status_block;

	NTSTATUS status = AfdSend(
		unwrap_handle(h.platform_handle),
		event,
		/* ApcRoutine: */ nullptr,
		/* ApcContext: */ nullptr,
		&io_status_block,
		&info);

	if (status == STATUS_PENDING)
	{
		status = event.wait_for_io(
			unwrap_handle(h.platform_handle),
			io_status_block,
			a.deadline);
	}

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return io_status_block.Information;
}
