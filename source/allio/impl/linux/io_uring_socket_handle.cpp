#include <allio/stream_socket_handle.hpp>
#include <allio/linux/io_uring_multiplexer.hpp>

#include <allio/implementation.hpp>
#include <allio/impl/linux/io_uring_byte_io.hpp>
#include <allio/impl/linux/io_uring_platform_handle.hpp>
#include <allio/impl/posix_socket.hpp>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::linux;

template<std::derived_from<common_socket_handle> Handle>
struct allio::multiplexer_handle_operation_implementation<io_uring_multiplexer, Handle, io::socket_create>
{
	using async_operation_storage = io_uring_multiplexer::basic_async_operation_storage<io::socket_create>;

	static constexpr bool block_synchronous = true;

	static result<void> start(io_uring_multiplexer& m, async_operation_storage& s)
	{
		allio_ASSERT(!*s.handle);
		allio_ASSERT((s.args.handle_flags & flags::multiplexable) != flags::none);
		allio_TRY(socket, create_socket(s.address_kind, s.args.handle_flags | flags::multiplexable));
		allio_TRYV(consume_socket_handle(*s.handle, { s.args.handle_flags }, std::move(socket)));
		m.post_synchronous_completion(s);
		return {};
	}
};

template<>
struct allio::multiplexer_handle_operation_implementation<io_uring_multiplexer, stream_socket_handle, io::socket_connect>
{
	struct async_operation_storage : io_uring_multiplexer::basic_async_operation_storage<io::socket_connect>
	{
		using basic_async_operation_storage::basic_async_operation_storage;

		socket_address addr;
	};

	static result<void> start(io_uring_multiplexer& m, async_operation_storage& s)
	{
		allio_ASSERT(*s.handle);
		allio_TRYA(s.addr, socket_address::make(s.address));

		return m.push(s, +[](async_operation_storage& s, io_uring_sqe& sqe)
		{
			sqe.opcode = IORING_OP_CONNECT;
			sqe.fd = unwrap_socket(s.handle->get_platform_handle());
			sqe.addr = reinterpret_cast<uintptr_t>(&s.addr.addr);
			sqe.off = s.addr.size;
		});
	}

	static result<void> cancel(io_uring_multiplexer& m, async_operation_storage& s)
	{
		return m.cancel(s);
	}
};

template<>
struct allio::multiplexer_handle_operation_implementation<io_uring_multiplexer, listen_socket_handle, io::socket_listen>
{
	using async_operation_storage = io_uring_multiplexer::basic_async_operation_storage<io::socket_listen>;

	static constexpr bool block_synchronous = true;

	static result<void> start(io_uring_multiplexer& m, async_operation_storage& s)
	{
		allio_ASSERT(*s.handle);
		allio_TRYV(listen_socket(unwrap_socket(s.handle->get_platform_handle()), s.address, s.args));
		m.post_synchronous_completion(s);
		return {};
	}
};

template<>
struct allio::multiplexer_handle_operation_implementation<io_uring_multiplexer, listen_socket_handle, io::socket_accept>
{
	struct async_operation_storage : io_uring_multiplexer::basic_async_operation_storage<io::socket_accept>
	{
		using basic_async_operation_storage::basic_async_operation_storage;

		socket_address addr;
	};

	static result<void> start(io_uring_multiplexer& m, async_operation_storage& s)
	{
		return m.push(s, +[](async_operation_storage& s, io_uring_sqe& sqe)
		{
			s.addr.size = sizeof(socket_address_union);

			sqe.opcode = IORING_OP_ACCEPT;
			sqe.fd = unwrap_socket(s.handle->get_platform_handle());
			sqe.addr = reinterpret_cast<uintptr_t>(&s.addr.addr);
			sqe.addr2 = reinterpret_cast<uintptr_t>(&s.addr.size);

			s.capture_result([](async_operation_storage& s, int const result) -> allio::result<void>
			{
				allio_ASSERT(!s.result->socket);
				allio_TRYV(consume_socket_handle(s.result->socket, { s.create_args.handle_flags }, unique_socket(result)));
				s.result->address = s.addr.get_network_address();
				return {};
			});
		});
	}

	static result<void> cancel(io_uring_multiplexer& m, async_operation_storage& s)
	{
		return m.cancel(s);
	}
};

allio_HANDLE_MULTIPLEXER_IMPLEMENTATION(io_uring_multiplexer, stream_socket_handle);
allio_HANDLE_MULTIPLEXER_IMPLEMENTATION(io_uring_multiplexer, listen_socket_handle);
