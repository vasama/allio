#pragma once

#include <allio/byte_io.hpp>
#include <allio/platform_handle.hpp>
#include <allio/static_multiplexer_handle_relation_provider.hpp>
#include <allio/linux/io_uring_multiplexer.hpp>
#include <allio/linux/platform.hpp>

#include <sys/uio.h>

#include <allio/linux/detail/undef.i>

namespace allio {

struct scatter_gather_async_operation_storage
	: linux::io_uring_multiplexer::async_operation_storage
	, io::scatter_gather_parameters
{
	handle const* handle;
	size_t* transferred;

	template<typename Parameters>
	scatter_gather_async_operation_storage(Parameters const& arguments, async_operation_listener* const listener)
		: async_operation_storage(arguments, listener)
		, io::scatter_gather_parameters(arguments)
		, handle(arguments.handle)
		, transferred(arguments.result)
	{
	}

	void init_sqe(io_uring_sqe& sqe, uint8_t const opcode)
	{
		auto const buffers = this->buffers.buffers();

		sqe.opcode = opcode;
		sqe.fd = linux::unwrap_handle(static_cast<platform_handle const*>(handle)->get_platform_handle());
		sqe.off = offset;
		sqe.addr = reinterpret_cast<uintptr_t>(buffers.data());
		sqe.len = buffers.size();

		capture_result([](scatter_gather_async_operation_storage& s, int const result)
		{
			*s.transferred = static_cast<size_t>(result);
		});
	}
};

template<std::derived_from<platform_handle> Handle>
struct multiplexer_handle_operation_implementation<linux::io_uring_multiplexer, Handle, io::scatter_read_at>
{
	using async_operation_storage = scatter_gather_async_operation_storage;

	static result<void> start(linux::io_uring_multiplexer& m, async_operation_storage& s)
	{
		return m.push(s, +[](async_operation_storage& s, io_uring_sqe& sqe)
		{
			s.init_sqe(sqe, IORING_OP_READV);
		});
	}

	static result<void> cancel(linux::io_uring_multiplexer& m, async_operation_storage& s)
	{
		return m.cancel(s);
	}
};

template<std::derived_from<platform_handle> Handle>
struct multiplexer_handle_operation_implementation<linux::io_uring_multiplexer, Handle, io::gather_write_at>
{
	using async_operation_storage = scatter_gather_async_operation_storage;

	static result<void> start(linux::io_uring_multiplexer& m, async_operation_storage& s)
	{
		return m.push(s, +[](async_operation_storage& s, io_uring_sqe& sqe)
		{
			s.init_sqe(sqe, IORING_OP_WRITEV);
		});
	}

	static result<void> cancel(linux::io_uring_multiplexer& m, async_operation_storage& s)
	{
		return m.cancel(s);
	}
};

template<std::derived_from<platform_handle> Handle>
struct multiplexer_handle_operation_implementation<linux::io_uring_multiplexer, Handle, io::stream_scatter_read>
{
	using async_operation_storage = scatter_gather_async_operation_storage;

	static result<void> start(linux::io_uring_multiplexer& m, async_operation_storage& s)
	{
		return m.push(s, +[](async_operation_storage& s, io_uring_sqe& sqe)
		{
			s.init_sqe(sqe, IORING_OP_READV);
		});
	}

	static result<void> cancel(linux::io_uring_multiplexer& m, async_operation_storage& s)
	{
		return m.cancel(s);
	}
};

template<std::derived_from<platform_handle> Handle>
struct multiplexer_handle_operation_implementation<linux::io_uring_multiplexer, Handle, io::stream_gather_write>
{
	using async_operation_storage = scatter_gather_async_operation_storage;

	static result<void> start(linux::io_uring_multiplexer& m, async_operation_storage& s)
	{
		return m.push(s, +[](async_operation_storage& s, io_uring_sqe& sqe)
		{
			s.init_sqe(sqe, IORING_OP_WRITEV);
		});
	}

	static result<void> cancel(linux::io_uring_multiplexer& m, async_operation_storage& s)
	{
		return m.cancel(s);
	}
};

} // namespace allio

#include <allio/linux/detail/undef.i>
