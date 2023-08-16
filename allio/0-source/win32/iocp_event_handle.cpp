#include <allio/event_handle.hpp>
#include <allio/win32/iocp_multiplexer.hpp>

#include <allio/implementation.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/win32/nt_error.hpp>

#include <allio/impl/win32/iocp_platform_handle.hpp>

using namespace allio;
using namespace allio::win32;

template<>
struct allio::multiplexer_handle_operation_implementation<iocp_multiplexer, event_handle, io::event_create>
{
	static constexpr bool block_synchronous = true;

	using async_operation_storage = basic_async_operation_storage<io::event_create, deferring_multiplexer::async_operation_storage>;

	static vsm::result<void> start(iocp_multiplexer& m, async_operation_storage& s)
	{
		return m.start(s, [&]() -> vsm::result<void>
		{
			return synchronous<event_handle>(s.args);
		});
	}
};

template<>
struct allio::multiplexer_handle_operation_implementation<iocp_multiplexer, event_handle, io::event_wait>
{
	static constexpr bool block_synchronous = true;

	struct async_operation_storage : basic_async_operation_storage<io::event_wait, iocp_multiplexer::async_operation_storage>
	{
		using basic_async_operation_storage::basic_async_operation_storage;

		iocp_multiplexer::wait_slot slot;

		void io_completed(iocp_multiplexer& m, iocp_multiplexer::io_slot&) override
		{
			set_result(NT_SUCCESS(slot->Status)
				? std::error_code()
				: std::error_code(static_cast<nt_error>(slot->Status)));
			m.post(*this, async_operation_status::concluded);
		}
	};

	//TODO: Implement deadline.
	static vsm::result<void> start(iocp_multiplexer& m, async_operation_storage& s)
	{
		return m.start(s, [&]() -> vsm::result<void>
		{
			event_handle const& h = *s.args.handle;

			if (!h)
			{
				return vsm::unexpected(error::handle_is_null);
			}

			s.slot.set_handler(s);
			vsm_try(signaled, m.start_wait(s.slot, h.get_platform_handle()));

			if (signaled)
			{
				s.set_result({});
				m.post(s, async_operation_status::concluded);
			}

			return {};
		});
	}

	static vsm::result<void> cancel(iocp_multiplexer& m, async_operation_storage& s)
	{
		vsm_try(cancelled, m.cancel_wait(s.slot));

		if (cancelled)
		{
			s.set_result(error::async_operation_cancelled);
			m.post(s, async_operation_status::concluded);
		}

		return {};
	}
};

allio_handle_multiplexer_implementation(iocp_multiplexer, event_handle);
