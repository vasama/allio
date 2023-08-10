#include <allio/impl/linux/event_handle.hpp>
#include <allio/linux/io_uring_multiplexer.hpp>

#include <allio/implementation.hpp>
#include <allio/impl/linux/event_handle.hpp>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::linux;

template<>
struct allio::multiplexer_handle_operation_implementation<io_uring_multiplexer, event_handle, io::event_create>
{
	static constexpr bool block_synchronous = true;

	using async_operation_storage = basic_async_operation_storage<io::event_create, deferring_multiplexer::async_operation_storage>;

	static vsm::result<void> start(io_uring_multiplexer& m, async_operation_storage& s)
	{
		return m.start(s, [&]() -> vsm::result<void>
		{
			return synchronous<event_handle>(s.args);
		});
	}
};

template<>
struct allio::multiplexer_handle_operation_implementation<io_uring_multiplexer, event_handle, io::event_wait>
{
	static constexpr bool block_synchronous = true;

	struct async_operation_storage : basic_async_operation_storage<io::event_wait, io_uring_multiplexer::async_operation_storage>
	{
		using basic_async_operation_storage::basic_async_operation_storage;

		void io_completed(io_uring_multiplexer& m, io_uring_multiplexer::io_data_ref, int const result) override
		{
			if (result >= 0)
			{
				vsm_assert(result == POLLIN);

				event_handle const& h = *s.args.handle;

				if (h.get_flags()[event_handle::flags::auto_reset])
				{
					set_result(vsm::as_error_code(vsm::discard_value(
						reset_event(unwrap_handle(h.get_platform_handle()))
					)));
				}
				else
				{
					set_result(std::error_code());
				}
			}
			else
			{
				set_result(static_cast<system_error>(result));
			}

			m.post(*this, async_operation_status::concluded);
		}
	};

	//TODO: Implement deadline.
	static vsm::result<void> start(io_uring_multiplexer& m, async_operation_storage& s)
	{
		return m.start(s, [&]() -> vsm::result<void>
		{
			event_handle const& h = *s.args.handle;

			if (!h)
			{
				return vsm::unexpected(error::handle_is_null);
			}
			
			return m.record_io([&](io_uring_multiplexer::submission_context& context) -> async_result<void>
			{
				if (s.args.deadline != deadline::never())
				{
					vsm_try_void(context.push_linked_timeout(s.timeout.set(s.args.deadline)));
				}

				//TODO: Use a linked read after the poll instead?
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

allio_handle_multiplexer_implementation(io_uring_multiplexer, event_handle);
