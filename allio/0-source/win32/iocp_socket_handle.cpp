#include <allio/socket_handle.hpp>
#include <allio/win32/iocp_multiplexer.hpp>

#include <allio/implementation.hpp>
#include <allio/impl/posix_socket.hpp>
#include <allio/impl/win32/iocp_byte_io.hpp>
#include <allio/impl/win32/iocp_platform_handle.hpp>
#include <allio/impl/win32/wsa_ex.hpp>

#include <vsm/unique_resource.hpp>
#include <vsm/utility.hpp>

using namespace allio;
using namespace allio::win32;

template<std::derived_from<common_socket_handle> Handle>
struct allio::multiplexer_handle_operation_implementation<iocp_multiplexer, Handle, io::socket_create>
{
	static constexpr bool block_synchronous = true;

	using async_operation_storage = basic_async_operation_storage<io::socket_create, deferring_multiplexer::async_operation_storage>;

	static vsm::result<void> start(iocp_multiplexer& m, async_operation_storage& s)
	{
		return m.start(s, [&]() -> vsm::result<void>
		{
			if (!s.args.multiplexable)
			{
				return vsm::unexpected(error::invalid_argument);
			}

			return synchronous<Handle>(s.args);
		});
	}
};

template<>
struct allio::multiplexer_handle_operation_implementation<iocp_multiplexer, stream_socket_handle, io::socket_connect>
{
	struct async_operation_storage : basic_async_operation_storage<io::socket_connect, iocp_multiplexer::async_operation_storage>
	{
		using basic_async_operation_storage::basic_async_operation_storage;

		iocp_multiplexer::overlapped slot;

		void io_completed(iocp_multiplexer& m, iocp_multiplexer::io_slot&) override
		{
			NTSTATUS const status = slot->Internal;
			set_result(nt_error(NT_SUCCESS(status) ? 0 : status));
			m.post(*this, async_operation_status::concluded);
		}
	};

	static vsm::result<void> start(iocp_multiplexer& m, async_operation_storage& s)
	{
		return m.start(s, [&]() -> async_result<void>
		{
			stream_socket_handle& h = *s.args.handle;

			if (!h)
			{
				return vsm::unexpected(error::handle_is_null);
			}

			SOCKET const socket = unwrap_socket(h.get_platform_handle());
			vsm_try(addr, socket_address::make(s.args.address));

			// Socket must be bound before calling ConnectEx.
			{
				socket_address_union bind_addr;
				memset(&bind_addr, 0, sizeof(bind_addr));
				bind_addr.addr.sa_family = addr.addr.sa_family;

				if (::bind(socket, &bind_addr.addr, addr.size))
				{
					return vsm::unexpected(get_last_socket_error());
				}
			}

			OVERLAPPED& overlapped = *s.slot;
			overlapped.Pointer = nullptr;
			overlapped.hEvent = NULL;

			s.slot.set_handler(s);
			if (DWORD const error = wsa_connect_ex(socket, addr, &overlapped))
			{
				if (error != ERROR_IO_PENDING)
				{
					return vsm::unexpected(socket_error(error));
				}
			}
			else if (m.supports_synchronous_completion(h))
			{
				s.set_result(std::error_code());
				m.post(s, async_operation_status::concluded);
			}

			return {};
		});
	}

	static vsm::result<void> cancel(iocp_multiplexer& m, async_operation_storage& s)
	{
		return m.cancel_io(s.slot, s.args.handle->get_platform_handle());
	}
};

template<>
struct allio::multiplexer_handle_operation_implementation<iocp_multiplexer, listen_socket_handle, io::socket_listen>
{
	static constexpr bool block_synchronous = true;

	using async_operation_storage = basic_async_operation_storage<io::socket_listen, deferring_multiplexer::async_operation_storage>;

	static vsm::result<void> start(iocp_multiplexer& m, async_operation_storage& s)
	{
		return m.start(s, [&]() -> vsm::result<void>
		{
			return synchronous<listen_socket_handle>(s.args);
		});
	}
};

template<>
struct allio::multiplexer_handle_operation_implementation<iocp_multiplexer, listen_socket_handle, io::socket_accept>
{
	struct async_operation_storage : basic_async_operation_storage<io::socket_accept, iocp_multiplexer::async_operation_storage>
	{
		using basic_async_operation_storage::basic_async_operation_storage;

		unique_socket_with_flags socket;
		wsa_accept_address_buffer output;
		iocp_multiplexer::overlapped slot;

		vsm::result<void> handle_success()
		{
			//TODO: Set completion notification modes depending on non-IFS LSP presence.

			vsm_try_void(initialize_socket_handle(
				args.result->socket, vsm_move(socket.socket),
				[&](native_platform_handle const socket_handle)
				{
					return platform_handle::native_handle_type
					{
						handle::native_handle_type
						{
							socket.flags | handle::flags::not_null
						},
						socket_handle,
					};
				}
			));

			args.result->address = output.remote.get_network_address();

			return {};
		}

		void io_completed(iocp_multiplexer& m, iocp_multiplexer::io_slot&) override
		{
			NTSTATUS const status = slot->Internal;
			set_result(NT_SUCCESS(status)
				? vsm::as_error_code(handle_success())
				: std::error_code(nt_error(status)));
			m.post(*this, async_operation_status::concluded);
		}
	};

	static vsm::result<void> start(iocp_multiplexer& m, async_operation_storage& s)
	{
		return m.start(s, [&]() -> async_result<void>
		{
			listen_socket_handle const& h = *s.args.handle;

			if (!h)
			{
				return vsm::unexpected(error::handle_is_null);
			}

			if (!s.args.multiplexable)
			{
				return vsm::unexpected(error::invalid_argument);
			}

			SOCKET const listen_socket = unwrap_socket(h.get_platform_handle());
			vsm_try(addr, socket_address::get(listen_socket));
			vsm_try_assign(s.socket, create_socket(addr.addr.sa_family, s.args));

			OVERLAPPED& overlapped = *s.slot;
			overlapped.Pointer = nullptr;
			overlapped.hEvent = NULL;

			s.slot.set_handler(s);
			if (DWORD const error = wsa_accept_ex(listen_socket, s.socket.socket.get(), s.output, &overlapped))
			{
				if (error != ERROR_IO_PENDING)
				{
					return vsm::unexpected(socket_error(error));
				}
			}
			else if (m.supports_synchronous_completion(h))
			{
				s.set_result(vsm::as_error_code(s.handle_success()));
				m.post(s, async_operation_status::concluded);
			}

			return {};
		});
	}

	static vsm::result<void> cancel(iocp_multiplexer& m, async_operation_storage& s)
	{
		return m.cancel_io(s.slot, s.args.handle->get_platform_handle());
	}
};

allio_handle_multiplexer_implementation(iocp_multiplexer, stream_socket_handle);
allio_handle_multiplexer_implementation(iocp_multiplexer, listen_socket_handle);
