#include <allio/impl/win32/process_handle.hpp>
#include <allio/win32/iocp_multiplexer.hpp>

#include <allio/implementation.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/win32/nt_error.hpp>

#include <allio/impl/win32/iocp_platform_handle.hpp>


using namespace allio;
using namespace allio::win32;

template<>
struct allio::multiplexer_handle_operation_implementation<iocp_multiplexer, process_handle, io::process_open>
{
	static constexpr bool block_synchronous = true;

	using async_operation_storage = basic_async_operation_storage<io::process_open, deferring_multiplexer::async_operation_storage>;

	static result<void> start(iocp_multiplexer& m, async_operation_storage& s)
	{
		return m.start(s, [&]() -> result<void>
		{
			return synchronous<process_handle>(s.args);
		});
	}
};

template<>
struct allio::multiplexer_handle_operation_implementation<iocp_multiplexer, process_handle, io::process_launch>
{
	static constexpr bool block_synchronous = true;

	using async_operation_storage = basic_async_operation_storage<io::process_launch, deferring_multiplexer::async_operation_storage>;

	static result<void> start(iocp_multiplexer& m, async_operation_storage& s)
	{
		return m.start(s, [&]() -> result<void>
		{
			return synchronous<process_handle>(s.args);
		});
	}
};

template<>
struct allio::multiplexer_handle_operation_implementation<iocp_multiplexer, process_handle, io::process_wait>
{
	static constexpr bool block_synchronous = true;

	struct async_operation_storage : basic_async_operation_storage<io::process_wait, iocp_multiplexer::async_operation_storage>
	{
		using basic_async_operation_storage::basic_async_operation_storage;

		iocp_multiplexer::wait_slot slot;

		result<void> handle_success()
		{
			return synchronous<process_handle>(args);
		}

		void io_completed(iocp_multiplexer& m, iocp_multiplexer::io_slot&) override
		{
			set_result(NT_SUCCESS(slot->Status)
				? as_error_code(handle_success())
				: std::error_code(nt_error(slot->Status)));
			m.post(*this, async_operation_status::concluded);
		}
	};

	static result<void> start(iocp_multiplexer& m, async_operation_storage& s)
	{
		return m.start(s, [&]() -> async_result<void>
		{
			process_handle& h = *s.args.handle;

			if (!h)
			{
				return allio_ERROR(error::handle_is_null);
			}

			if (h.is_current())
			{
				return allio_ERROR(error::process_is_current_process);
			}

			s.slot.set_handler(s);
			allio_TRY(completed, m.start_wait(s.slot, h.get_platform_handle()));
			if (completed)
			{
				s.set_result(as_error_code(s.handle_success()));
				m.post(s, async_operation_status::concluded);
			}
			return {};
		});
	}

	static result<void> cancel(iocp_multiplexer& m, async_operation_storage& s)
	{
		allio_TRY(cancelled, m.cancel_wait(s.slot));
		if (cancelled)
		{
			s.set_result(error::async_operation_cancelled);
			m.post(s, async_operation_status::concluded);
		}
		return {};
	}
};

allio_HANDLE_MULTIPLEXER_IMPLEMENTATION(iocp_multiplexer, process_handle);
