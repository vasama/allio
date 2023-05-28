#pragma once

#include <allio/impl/linux/platform_handle.hpp>
#include <allio/linux/io_uring_multiplexer.hpp>

#include <allio/impl/linux/error.hpp>
#include <allio/implementation.hpp>

#include <linux/io_uring.h>

#include <allio/linux/detail/undef.i>

namespace allio {

template<std::derived_from<platform_handle> Handle>
struct multiplexer_handle_implementation<linux::io_uring_multiplexer, Handle>
{
	static vsm::result<void> register_handle(linux::io_uring_multiplexer& m, Handle const& h)
	{
		return m.register_native_handle(h.get_platform_handle());
	}

	static vsm::result<void> deregister_handle(linux::io_uring_multiplexer& m, Handle const& h)
	{
		return m.deregister_native_handle(h.get_platform_handle());
	}
};

template<std::derived_from<platform_handle> Handle>
struct multiplexer_handle_operation_implementation<linux::io_uring_multiplexer, Handle, io::close>
{
	struct async_operation_storage : basic_async_operation_storage<io::close, linux::io_uring_multiplexer::async_operation_storage>
	{
		using basic_async_operation_storage::basic_async_operation_storage;

		void io_completed(linux::io_uring_multiplexer& m, linux::io_uring_multiplexer::io_data_ref, int const result) override
		{
			set_result(static_cast<linux::system_error>(result));
			m.post(*this, async_operation_status::concluded);
		}
	};

	static vsm::result<void> start(linux::io_uring_multiplexer& m, async_operation_storage& s)
	{
		return m.start(s, [&]() -> async_result<void>
		{
			platform_handle& h = static_cast<platform_handle&>(*s.args.handle);

			if (!h)
			{
				return vsm::unexpected(error::handle_is_null);
			}

			return m.record_io(s, [&](io_uring_sqe& sqe)
			{
				sqe.opcode = IORING_OP_CLOSE;
				sqe.fd = linux::unwrap_handle(h.get_platform_handle());
			});
		});
	}
};

} // namespace allio

#include <allio/linux/detail/undef.i>
