#include <allio/impl/linux/process_handle.hpp>
#include <allio/linux/io_uring_multiplexer.hpp>

#include <allio/implementation.hpp>

#include <poll.h>

#include <allio/linux/detail/undef.i>

namespace allio;
namespace allio::linux;

template<>
struct multiplexer_handle_operation_implementation<process_handle, io::process_open>
{
	using async_operation_storage = basic_async_operation_storage<io_uring_multiplexer, io::process_open>;

	static constexpr bool block_synchronous = true;
	static constexpr bool start_synchronous = true;
};

template<>
struct multiplexer_handle_operation_implementation<process_handle, io::process_launch>
{
	using async_operation_storage = basic_async_operation_storage<io_uring_multiplexer, io::process_launch>;

	static constexpr bool block_synchronous = true;
	static constexpr bool start_synchronous = true;
};

template<>
struct multiplexer_handle_operation_implementation<process_handle, io::process_wait>
{
	using async_operation_storage = basic_async_operation_storage<io_uring_multiplexer, io::process_wait>;

	static constexpr bool block_synchronous = true;

	static vsm::result<void> start(io_uring_multiplexer& m, async_operation_storage& s)
	{
		process_handle& h = *s.handle;

		if (!h)
		{
			return vsm::unexpected(error::handle_is_null);
		}

		if ((h.m_flags.value & process_flags::current) != process_flags::none)
		{
			return vsm::unexpected(error::process_is_current_process);
		}

		if ((h.m_flags.value & process_flags::exited) == process_flags::none)
		{
			m.push(s, +[](async_operation_storage& s, io_uring_sqe& sqe)
			{
				sqe.opcode = IORING_OP_POLL_ADD;
				sqe.fd = unwrap_socket(h.get_platform_handle());
				sqe.poll_events = EPOLLIN;

				s.capture_result([](async_operation_storage& s, int const result) -> vsm::result<void>
				{
					vsm_assert(result == EPOLLIN);

					// Use the synchronous implementation with an instant deadline to retrieve the exit code.
					s.deadline = deadline::instant();
					return synchronous_operation_implementation<process_handle, io::process_wait>(s);
				});
			});
		}
		else
		{
			*s.result = h.m_exit_code.value;
			m.post_synchronous_completion(s);
		}

		return {};
	}

	static vsm::result<void> cancel(io_uring_multiplexer& m, async_operation_storage& s)
	{
		return m.cancel(s);
	}
};

allio_handle_multiplexer_implementation(io_uring_multiplexer, process_handle);
