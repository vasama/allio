#include <allio/socket_handle.hpp>
#include <allio/win32/iocp_multiplexer.hpp>

#include <allio/detail/unique_resource.hpp>
#include <allio/static_multiplexer_handle_relation_provider.hpp>

#include "iocp_byte_io.hpp"
#include "iocp_platform_handle.hpp"
#include "../posix_socket.hpp"
#include "wsa_ex.hpp"

using namespace allio;
using namespace allio::win32;

template<std::derived_from<detail::common_socket_handle_base> Handle>
struct allio::multiplexer_handle_operation_implementation<iocp_multiplexer, Handle, io::socket>
{
	using async_operation_storage = iocp_multiplexer::basic_async_operation_storage<io::socket>;

	static constexpr bool synchronous = true;

	static result<void> start(iocp_multiplexer& m, async_operation_storage& s)
	{
		allio_ASSERT(!*s.handle);
		allio_ASSERT((s.args.handle_flags & flags::multiplexable) != flags::none);
		allio_TRY(socket, create_socket(s.address_kind, s.args.handle_flags));
		allio_TRYV(consume_socket_handle(*s.handle, { s.args.handle_flags }, std::move(socket)));
		m.post_synchronous_completion(s);
		return {};
	}
};

template<>
struct allio::multiplexer_handle_operation_implementation<iocp_multiplexer, socket_handle, io::connect>
{
	using async_operation_storage = iocp_multiplexer::basic_async_operation_storage<io::connect>;

	static result<void> start(iocp_multiplexer& m, async_operation_storage& s)
	{
		allio_ASSERT(*s.handle);

		allio_TRY(addr, socket_address::make(s.address));

		allio_TRY(io_slot, m.acquire_io_slot());

		SOCKET const socket = unwrap_socket(s.handle->get_platform_handle());

		/* Socket must be bound before ConnectEx. */ {
			socket_address_union bind_addr;
			memset(&bind_addr, 0, sizeof(bind_addr));
			bind_addr.addr.sa_family = addr.addr.sa_family;

			if (::bind(socket, &bind_addr.addr, addr.size))
			{
				return allio_ERROR(get_last_socket_error());
			}
		}

		if (DWORD const error = wsa_connect_ex(socket, addr, &io_slot.bind(s).overlapped()))
		{
			if (error != ERROR_IO_PENDING)
			{
				return allio_ERROR(std::error_code(error, wsa_error_category::get()));
			}
		}
		else if (m.supports_synchronous_completion(*s.handle))
		{
			m.post_synchronous_completion(s);
		}

		return {};
	}

	static result<void> cancel(iocp_multiplexer& m, async_operation_storage& s)
	{
		return m.cancel(s.handle->get_platform_handle(), s);
	}
};

template<>
struct allio::multiplexer_handle_operation_implementation<iocp_multiplexer, listen_socket_handle, io::listen>
{
	using async_operation_storage = iocp_multiplexer::basic_async_operation_storage<io::listen>;

	static constexpr bool synchronous = true;

	static result<void> start(iocp_multiplexer& m, async_operation_storage& s)
	{
		allio_ASSERT(*s.handle);
		allio_TRYV(listen_socket(unwrap_socket(s.handle->get_platform_handle()), s.address, s.args));
		m.post_synchronous_completion(s);
		return {};
	}
};

template<>
struct allio::multiplexer_handle_operation_implementation<iocp_multiplexer, listen_socket_handle, io::accept>
{
	struct async_operation_storage : iocp_multiplexer::basic_async_operation_storage<io::accept>
	{
		using basic_async_operation_storage::basic_async_operation_storage;

		unique_socket socket;
		wsa_accept_address_buffer output;
	};

	static result<void> start(iocp_multiplexer& m, async_operation_storage& s)
	{
		if ((s.create_args.handle_flags & flags::multiplexable) == flags::none)
		{
			return allio_ERROR(error::handle_is_not_multiplexable);
		}

		SOCKET const listen_socket = unwrap_socket(s.handle->get_platform_handle());

		allio_TRY(addr, socket_address::get(listen_socket));

		allio_TRY(io_slot, m.acquire_io_slot());
		allio_TRYA(s.socket, create_socket(addr.addr.sa_family, s.create_args.handle_flags));

		s.capture_information([](async_operation_storage& s, uintptr_t) -> result<void>
		{
			allio_TRYV(consume_socket_handle(s.result->socket, { s.create_args.handle_flags }, std::move(s.socket)));
			s.result->address = s.output.remote.get_network_address();
			return {};
		});

		if (DWORD const error = wsa_accept_ex(listen_socket, s.socket.get(), s.output, &io_slot.bind(s).overlapped()))
		{
			if (error != ERROR_IO_PENDING)
			{
				return allio_ERROR(std::error_code(error, wsa_error_category::get()));
			}
		}
		else if (m.supports_synchronous_completion(*s.handle))
		{
			m.post_synchronous_completion(s);
		}

		return {};
	}

	static result<void> cancel(iocp_multiplexer& m, async_operation_storage& s)
	{
		return m.cancel(s.handle->get_platform_handle(), s);
	}
};

template<std::derived_from<detail::common_socket_handle_base> Handle>
	requires std::derived_from<Handle, platform_handle>
struct allio::multiplexer_handle_operation_implementation<iocp_multiplexer, Handle, io::close>
{
	using async_operation_storage = iocp_multiplexer::basic_async_operation_storage<io::close>;

	static constexpr bool synchronous = true;

	static result<void> start(iocp_multiplexer& m, async_operation_storage& s)
	{
		allio_TRY(handle, static_cast<Handle*>(s.handle)->release_native_handle());
		allio_VERIFY(close_socket(unwrap_socket(handle.handle)));
		m.post_synchronous_completion(s);
		return {};
	}
};

allio_MULTIPLEXER_HANDLE_RELATION(iocp_multiplexer, socket_handle);
allio_MULTIPLEXER_HANDLE_RELATION(iocp_multiplexer, listen_socket_handle);
