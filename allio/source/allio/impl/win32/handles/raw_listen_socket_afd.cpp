#include <allio/detail/handles/raw_socket.hpp>

#include <allio/impl/posix/socket.hpp>
#include <allio/impl/win32/afd.hpp>

#include <vsm/out_resource.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

using accept_result_type = accept_result<basic_detached_handle<raw_socket_t>>;

vsm::result<void> raw_listen_socket_t::listen(
	native_handle<raw_listen_socket_t>& h,
	io_parameters_t<raw_listen_socket_t, listen_t> const& a)
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
			vsm::unexpected(static_cast<kernel_error>(status));
		}
	}

	// Bind
	{
		NTSTATUS const status = AfdBind(
			handle.get(),
			AFD_SHARE_UNIQUE,
			&addr.addr,
			static_cast<ULONG>(addr.size));

		if (!NT_SUCCESS(status))
		{
			return vsm::unexpected(static_cast<kernel_error>(status));
		}
	}

	// Listen
	{
		NTSTATUS const status = AfdListen(
			handle.get(),
			a.backlog);

		if (!NT_SUCCESS(status))
		{
			return vsm::unexpected(static_cast<kernel_error>(status));
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

vsm::result<accept_result_type> raw_listen_socket_t::accept(
	native_handle<raw_listen_socket_t> const& h,
	io_parameters_t<raw_listen_socket_t, accept_t> const& a)
{
	vsm_try(event, thread_event::get_for(h));

	struct
	{
		AFD_RECEIVED_ACCEPT_DATA data;
		posix::socket_address_union addr;
	}
	data;

	// Wait
	{
		IO_STATUS_BLOCK io_status_block;

		NTSTATUS status = AfdWaitForAccept(
			unwrap_handle(h.platform_handle),
			/* Event: */ NULL,
			/* ApcRoutine: */ nullptr,
			/* ApcContext: */ nullptr,
			&io_status_block,
			&data.data,
			sizeof(data));

		if (status == STATUS_PENDING)
		{
			status = io_wait(
				unwrap_handle(h.platform_handle),
				/* event: */ NULL,
				io_status_block,
				a.deadline);
		}

		if (!NT_SUCCESS(status))
		{
			return vsm::unexpected(static_cast<kernel_error>(status));
		}
	}

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
			vsm::unexpected(static_cast<kernel_error>(status));
		}
	}

	// Accept
	{
		IO_STATUS_BLOCK io_status_block;

		NTSTATUS status = AfdAccept(
			unwrap_handle(h.platform_handle),
			/* Event: */ NULL,
			/* ApcRoutine: */ nullptr,
			/* ApcContext: */ nullptr,
			&io_status_block,
			handle.get(),
			data.data.SequenceNumber);

		if (status == STATUS_PENDING)
		{
			status = io_wait(
				unwrap_handle(h.platform_handle),
				/* event: */ NULL,
				io_status_block,
				a.deadline);
		}

		if (!NT_SUCCESS(status))
		{
			return vsm::unexpected(static_cast<kernel_error>(status));
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

	auto const make_socket_handle = [&]()
	{
		native_handle<raw_socket_t> h = {};
		h.flags = object_t::flags::not_null | h_flags;
		h.platform_handle = wrap_handle(handle.release());
		return basic_detached_handle<raw_socket_t>(adopt_handle, h);
	};

	return vsm::result<accept_result_type>(
		vsm::result_value,
		vsm_lazy(make_socket_handle()),
		addr.get_network_endpoint());
}
