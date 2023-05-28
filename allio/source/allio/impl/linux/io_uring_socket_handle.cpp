#include <allio/stream_socket_handle.hpp>
#include <allio/linux/io_uring_multiplexer.hpp>

#include <allio/implementation.hpp>
#include <allio/impl/linux/io_uring_byte_io.hpp>
#include <allio/impl/linux/io_uring_platform_handle.hpp>
#include <allio/impl/linux/version.hpp>
#include <allio/impl/posix_socket.hpp>

#include <vsm/utility.hpp>

#if LINUX_KERNEL_CODE < KERNEL_VERSION(5, 19, 0)
static constexpr decltype(IORING_OP_NOP) IORING_OP_SOCKET = decltype(IORING_OP_NOP)(45);
#endif

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::linux;

template<std::derived_from<common_socket_handle> Handle>
struct allio::multiplexer_handle_operation_implementation<io_uring_multiplexer, Handle, io::socket_create>
{
	static constexpr bool block_synchronous = true;

	struct async_operation_storage : basic_async_operation_storage<io::socket_create, io_uring_multiplexer::async_operation_storage>
	{
		using basic_async_operation_storage::basic_async_operation_storage;

		void io_completed(io_uring_multiplexer& m, io_uring_multiplexer::io_data_ref, int const result) override
		{
			Handle& h = static_cast<Handle&>(*args.handle);

			vsm_assert(!h &&
				"common_socket_handle was initialized during asynchronous create via io uring.");

			if (result >= 0)
			{
				vsm_verify(h.set_native_handle(
					common_socket_handle::native_handle_type
					{
						platform_handle::native_handle_type
						{
							handle::native_handle_type
							{
								handle::flags::not_null,
							},
							wrap_handle(result),
						}
					}
				));

				set_result(std::error_code());
			}
			else
			{
				set_result(static_cast<system_error>(-result));
			}

			m.post(*this, async_operation_status::concluded);
		}
	};

	static vsm::result<void> start(io_uring_multiplexer& m, async_operation_storage& s)
	{
		return m.start(s, [&]() -> async_result<void>
		{
			if (get_kernel_version() < KERNEL_VERSION(5, 19, 0))
			{
				return synchronous<Handle>(s.args);
			}

			common_socket_handle& h = *s.args.handle;

			if (h)
			{
				return vsm::unexpected(error::handle_is_not_null);
			}

			if (!s.args.multiplexable)
			{
				return vsm::unexpected(error::invalid_argument);
			}

			vsm_try(address_family, get_address_family(s.args.address_kind));

			return m.record_io(s, [&](io_uring_sqe& sqe)
			{
				sqe.opcode = IORING_OP_SOCKET;
				sqe.fd = address_family;
				sqe.off = SOCK_STREAM; //TODO: Support packet socket
				sqe.len = IPPROTO_TCP; //TODO: Support UDP
			});
		});
	}
};

template<>
struct allio::multiplexer_handle_operation_implementation<io_uring_multiplexer, stream_socket_handle, io::socket_connect>
{
	struct async_operation_storage : basic_async_operation_storage<io::socket_connect, io_uring_multiplexer::async_operation_storage>
	{
		using basic_async_operation_storage::basic_async_operation_storage;

		socket_address addr;
	};

	static vsm::result<void> start(io_uring_multiplexer& m, async_operation_storage& s)
	{
		return m.start(s, [&]() -> async_result<void>
		{
			stream_socket_handle& h = *s.args.handle;

			if (!h)
			{
				return vsm::unexpected(error::handle_is_null);
			}

			vsm_try_assign(s.addr, socket_address::make(s.args.address));

			return m.record_io(s, [&](io_uring_sqe& sqe)
			{
				sqe.opcode = IORING_OP_CONNECT;
				sqe.fd = unwrap_socket(h.get_platform_handle());
				sqe.addr = reinterpret_cast<uintptr_t>(&s.addr.addr);
				sqe.off = s.addr.size;
			});
		});
	}

	static vsm::result<void> cancel(io_uring_multiplexer& m, async_operation_storage& s)
	{
		return m.cancel_io(s);
	}
};

template<>
struct allio::multiplexer_handle_operation_implementation<io_uring_multiplexer, listen_socket_handle, io::socket_listen>
{
	static constexpr bool block_synchronous = true;

	using async_operation_storage = basic_async_operation_storage<io::socket_listen, io_uring_multiplexer::async_operation_storage>;

	static vsm::result<void> start(io_uring_multiplexer& m, async_operation_storage& s)
	{
		return m.start(s, [&]() -> vsm::result<void>
		{
			return synchronous<listen_socket_handle>(s.args);
		});
	}
};

template<>
struct allio::multiplexer_handle_operation_implementation<io_uring_multiplexer, listen_socket_handle, io::socket_accept>
{
	struct async_operation_storage : basic_async_operation_storage<io::socket_accept, io_uring_multiplexer::async_operation_storage>
	{
		using basic_async_operation_storage::basic_async_operation_storage;

		socket_address addr;

		void io_completed(io_uring_multiplexer& m, io_uring_multiplexer::io_data_ref const data, int const result) override
		{
			args.result->address = addr.get_network_address();
			basic_async_operation_storage::io_completed(m, data, result);
		}
	};

	static vsm::result<void> start(io_uring_multiplexer& m, async_operation_storage& s)
	{
		return m.start(s, [&]() -> async_result<void>
		{
			listen_socket_handle const& h = *s.args.handle;

			if (!h)
			{
				return vsm::unexpected(error::handle_is_null);
			}

			return m.record_io(s, [&](io_uring_sqe& sqe)
			{
				s.addr.size = sizeof(socket_address_union);

				sqe.opcode = IORING_OP_ACCEPT;
				sqe.fd = unwrap_socket(h.get_platform_handle());
				sqe.addr = reinterpret_cast<uintptr_t>(&s.addr.addr);
				sqe.addr2 = reinterpret_cast<uintptr_t>(&s.addr.size);
			});
		});
	}

	static vsm::result<void> cancel(io_uring_multiplexer& m, async_operation_storage& s)
	{
		return m.cancel_io(s);
	}
};

allio_handle_multiplexer_implementation(io_uring_multiplexer, stream_socket_handle);
allio_handle_multiplexer_implementation(io_uring_multiplexer, listen_socket_handle);
