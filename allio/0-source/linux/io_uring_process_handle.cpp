#include <allio/impl/linux/process_handle.hpp>
#include <allio/linux/io_uring_multiplexer.hpp>

#include <allio/impl/linux/error.hpp>
#include <allio/implementation.hpp>
#include <allio/linux/platform.hpp>

#include <allio/impl/linux/io_uring_platform_handle.hpp>

#include <linux/io_uring.h>
#include <poll.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::linux;

template<>
struct allio::multiplexer_handle_operation_implementation<io_uring_multiplexer, process_handle, io::process_open>
{
	static constexpr bool block_synchronous = true;

	using async_operation_storage = basic_async_operation_storage<io::process_open, deferring_multiplexer::async_operation_storage>;

	static vsm::result<void> start(io_uring_multiplexer& m, async_operation_storage& s)
	{
		return m.start(s, [&]() -> vsm::result<void>
		{
			return synchronous<process_handle>(s.args);
		});
	}
};

template<>
struct allio::multiplexer_handle_operation_implementation<io_uring_multiplexer, process_handle, io::process_launch>
{
	static constexpr bool block_synchronous = true;

	using async_operation_storage = basic_async_operation_storage<io::process_launch, deferring_multiplexer::async_operation_storage>;

	static vsm::result<void> start(io_uring_multiplexer& m, async_operation_storage& s)
	{
		return m.start(s, [&]() -> vsm::result<void>
		{
			return synchronous<process_handle>(s.args);
		});
	}
};

template<>
struct allio::multiplexer_handle_operation_implementation<io_uring_multiplexer, process_handle, io::process_wait>
{
	static constexpr bool block_synchronous = true;

	struct async_operation_storage : basic_async_operation_storage<io::process_wait, io_uring_multiplexer::async_operation_storage>
	{
		using basic_async_operation_storage::basic_async_operation_storage;

		io_uring_multiplexer::timeout timeout;


		vsm::result<void> get_exit_code() const
		{
			//TODO: Use waitid to get exit result.
			return {};
		}

		void io_completed(io_uring_multiplexer& m, io_uring_multiplexer::io_data_ref, int const result) override
		{
			if (result >= 0)
			{
				vsm_assert(result == POLLIN);
				set_result(vsm::as_error_code(get_exit_code()));
			}
			else
			{
				set_result(static_cast<system_error>(result));
			}

			m.post(*this, async_operation_status::concluded);
		}
	};

	static vsm::result<void> start(io_uring_multiplexer& m, async_operation_storage& s)
	{
		return m.start(s, [&]() -> async_result<void>
		{
			process_handle& h = *s.args.handle;

			if (!h)
			{
				return vsm::unexpected(error::handle_is_null);
			}

			if (h.is_current())
			{
				return vsm::unexpected(error::process_is_current_process);
			}

			if (h.get_flags()[process_handle::flags::exited])
			{
				//TODO: Set exit code.
				//*s.args.result = h.m_exit_code.value;
				s.set_result(std::error_code());
				m.post(s, async_operation_status::concluded);

				return {};
			}

			if (s.args.deadline == deadline::instant())
			{
				return s.get_exit_code();
			}

			return m.record_io([&](io_uring_multiplexer::submission_context& context) -> async_result<void>
			{
				if (s.args.deadline != deadline::never())
				{
					vsm_try_void(context.push_linked_timeout(s.timeout.set(s.args.deadline)));
				}

				return context.push(s, [&](io_uring_sqe& sqe)
				{
					sqe.opcode = IORING_OP_POLL_ADD;
					sqe.fd = unwrap_handle(h.get_platform_handle());
					sqe.poll_events = POLLIN;
				});
			});
		});
	}

	static vsm::result<void> cancel(io_uring_multiplexer& m, async_operation_storage& s)
	{
		return m.cancel_io(s);
	}
};

allio_handle_multiplexer_implementation(io_uring_multiplexer, process_handle);
