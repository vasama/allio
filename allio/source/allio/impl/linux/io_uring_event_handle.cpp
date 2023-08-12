#include <allio/impl/linux/event_handle.hpp>
#include <allio/linux/io_uring_multiplexer.hpp>

#include <allio/implementation.hpp>
#include <allio/impl/linux/error.hpp>
#include <allio/linux/platform.hpp>
#include <allio/step_deadline.hpp>

#include <allio/impl/linux/io_uring_platform_handle.hpp>

#include <linux/io_uring.h>
#include <poll.h>
#include <sys/eventfd.h>

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

		step_deadline absolute_deadline;
		io_uring_multiplexer::timeout timeout;

		io_uring_multiplexer::io_slot read_slot;
		eventfd_t read_value;


		async_result<void> submit_io(io_uring_multiplexer& m)
		{
			event_handle const& h = *args.handle;

			return m.record_io([&](io_uring_multiplexer::submission_context& context) -> async_result<void>
			{
				vsm_try(relative_deadline, absolute_deadline.step());

				if (relative_deadline != deadline::never())
				{
					vsm_try_void(context.push_linked_timeout(timeout.set(relative_deadline)));
				}

				int const event = unwrap_handle(h.get_platform_handle());
				bool const auto_reset = h.get_flags()[event_handle::flags::auto_reset];

				vsm_try_void(context.push(*this, [&](io_uring_sqe& sqe)
				{
					sqe.opcode = IORING_OP_POLL_ADD;
					sqe.fd = event;
					sqe.poll_events = POLLIN;

					if (auto_reset)
					{
						sqe.flags = IOSQE_IO_LINK;
					}
				}));

				if (auto_reset)
				{
					vsm_try_void(context.push(*this, [&](io_uring_sqe& sqe)
					{
						sqe.opcode = IORING_OP_READ;
						sqe.addr = reinterpret_cast<uintptr_t>(&read_value);
						sqe.fd = event;
						sqe.len = sizeof(read_value);
						sqe.flags = IOSQE_IO_LINK;
					}));
				}

				return {};
			});
		}

		vsm::result<void> cancel_io(io_uring_multiplexer& m)
		{
			
		}

		void io_completed(io_uring_multiplexer& m, io_uring_multiplexer::io_data_ref const slot, int const result) override
		{
			if (slot == *this)
			{
				if (result >= 0)
				{
					vsm_assert(result == POLLIN);

					if (!h.get_flags()[event_handle::flags::auto_reset])
					{
						set_result(std::error_code());
						m.post(*this, async_operation_status::concluded);
					}
				}
				else
				{
					set_result(static_cast<system_error>(result));
					m.post(*this, async_operation_status::concluded);
				}
			}
			else
			{
				if (result >= 0)
				{
					vsm_assert(result == sizeof(read_value));
					vsm_assert(read_value != 0);

					set_result(std::error_code());
					m.post(*this, async_operation_status::concluded);
				}
				else
				{
					if (result == EAGAIN)
					{
						auto const r = submit_io(m);

						if (!r)
						{
							//TODO: Is it reasonable that this return a busy error
							//      when the io uring submission queue is full?
							set_result(r.error());
							m.post(*this, async_operation_status::concluded);
						}
					}
					else
					{
						set_result(static_cast<system_error>(result));
						m.post(*this, async_operation_status::concluded);
					}
				}
			}
		}
	};

	//TODO: Implement deadline.
	static vsm::result<void> start(io_uring_multiplexer& m, async_operation_storage& s)
	{
		return m.start(s, [&]() -> async_result<void>
		{
			event_handle const& h = *s.args.handle;

			if (!h)
			{
				return vsm::unexpected(error::handle_is_null);
			}

			s.absolute_deadline = s.args.deadline;

			return s.start_wait(m);
		});
	}

	static vsm::result<void> cancel(io_uring_multiplexer& m, async_operation_storage& s)
	{
		return m.cancel_io(s);
	}
};

allio_handle_multiplexer_implementation(io_uring_multiplexer, event_handle);
