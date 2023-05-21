#pragma once

#include <allio/impl/linux/platform_handle.hpp>
#include <allio/linux/io_uring_multiplexer.hpp>

#include <allio/implementation.hpp>

#include <allio/linux/detail/undef.i>

namespace allio {

template<std::derived_from<platform_handle> Handle>
struct allio::multiplexer_handle_implementation<linux::io_uring_multiplexer, Handle>
{
	static vsm::result<void*> register_handle(linux::io_uring_multiplexer& m, Handle const& h)
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
	using async_operation_storage = linux::io_uring_multiplexer::basic_async_operation_storage<io::close>;

	static vsm::result<void> start(linux::io_uring_multiplexer& m, async_operation_storage& s)
	{
		return m.push(s, +[](async_operation_storage& s, io_uring_sqe& sqe)
		{
			sqe.opcode = IORING_OP_CLOSE;
			sqe.fd = linux::unwrap_handle(static_cast<platform_handle*>(s.handle)->get_platform_handle());
		});
	}

	static vsm::result<void> cancel(linux::io_uring_multiplexer& m, async_operation_storage& s)
	{
		return m.cancel(s);
	}
};

} // namespace allio

#include <allio/linux/detail/undef.i>
