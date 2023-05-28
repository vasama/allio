#pragma once

#include <allio/byte_io.hpp>
#include <allio/implementation.hpp>
#include <allio/platform_handle.hpp>
#include <allio/linux/io_uring_multiplexer.hpp>
#include <allio/linux/platform.hpp>

#include <linux/io_uring.h>
#include <sys/uio.h>

#include <allio/linux/detail/undef.i>

namespace allio {
namespace linux {

class io_uring_scatter_gather_async_operation_storage
	: public io_uring_multiplexer::async_operation_storage
{
	struct
	{
		allio::deadline deadline;
		detail::untyped_buffers_storage buffers;
		file_size offset;
		platform_handle const* handle;
		size_t* result;
	}
	m_args;

public:
	explicit io_uring_scatter_gather_async_operation_storage(auto const& args, async_operation_listener* const listener)
		: io_uring_multiplexer::async_operation_storage(listener)
		, m_args
		{
			args.deadline,
			args.buffers,
			args.offset,
			static_cast<platform_handle const*>(args.handle),
			args.result,
		}
	{
	}

	//TODO: Pass supports_cancellation = false for disk I/O?
	// If so, how can we detect *disk* I/O as opposed to e.g. network file I/O?
	// Linux isn't actually capable of cancelling disk I/O once started.
	async_result<void> record(io_uring_multiplexer& m, uint8_t const opcode, bool const supports_cancellation = true)
	{
		return m.record_io([&](io_uring_multiplexer::submission_context& ctx) -> async_result<void>
		{
			if (supports_cancellation && !m_args.deadline.is_trivial())
			{
				vsm_try_void(ctx.push_linked_timeout(m_args.deadline));
			}

			return ctx.push(*this, [&](io_uring_sqe& sqe)
			{
				auto const buffers = m_args.buffers.buffers();

				sqe.opcode = opcode;
				sqe.fd = unwrap_handle(m_args.handle->get_platform_handle());
				sqe.off = m_args.offset;
				sqe.addr = reinterpret_cast<uintptr_t>(buffers.data());
				sqe.len = buffers.size();
			});
		});
	}

private:
	void io_completed(io_uring_multiplexer& m, io_uring_multiplexer::io_data_ref, int const result) override
	{
		if (result >= 0)
		{
			*m_args.result = static_cast<size_t>(result);
			set_result(std::error_code());
		}
		else
		{
			set_result(std::error_code(-result, std::system_category()));
		}

		m.post(*this, async_operation_status::concluded);
	}
};

} // namespace linux

template<std::derived_from<platform_handle> Handle>
struct multiplexer_handle_operation_implementation<linux::io_uring_multiplexer, Handle, io::scatter_read_at>
{
	using async_operation_storage = linux::io_uring_scatter_gather_async_operation_storage;

	static vsm::result<void> start(linux::io_uring_multiplexer& m, async_operation_storage& s)
	{
		return m.start(s, [&] { return s.record(m, IORING_OP_READV); });
	}

	static vsm::result<void> cancel(linux::io_uring_multiplexer& m, async_operation_storage& s)
	{
		return m.cancel_io(s);
	}
};

template<std::derived_from<platform_handle> Handle>
struct multiplexer_handle_operation_implementation<linux::io_uring_multiplexer, Handle, io::gather_write_at>
{
	using async_operation_storage = linux::io_uring_scatter_gather_async_operation_storage;

	static vsm::result<void> start(linux::io_uring_multiplexer& m, async_operation_storage& s)
	{
		return m.start(s, [&] { return s.record(m, IORING_OP_WRITEV); });
	}

	static vsm::result<void> cancel(linux::io_uring_multiplexer& m, async_operation_storage& s)
	{
		return m.cancel_io(s);
	}
};

template<std::derived_from<platform_handle> Handle>
struct multiplexer_handle_operation_implementation<linux::io_uring_multiplexer, Handle, io::stream_scatter_read>
{
	using async_operation_storage = linux::io_uring_scatter_gather_async_operation_storage;

	static vsm::result<void> start(linux::io_uring_multiplexer& m, async_operation_storage& s)
	{
		return m.start(s, [&] { return s.record(m, IORING_OP_READV); });
	}

	static vsm::result<void> cancel(linux::io_uring_multiplexer& m, async_operation_storage& s)
	{
		return m.cancel_io(s);
	}
};

template<std::derived_from<platform_handle> Handle>
struct multiplexer_handle_operation_implementation<linux::io_uring_multiplexer, Handle, io::stream_gather_write>
{
	using async_operation_storage = linux::io_uring_scatter_gather_async_operation_storage;

	static vsm::result<void> start(linux::io_uring_multiplexer& m, async_operation_storage& s)
	{
		return m.start(s, [&] { return s.record(m, IORING_OP_WRITEV); });
	}

	static vsm::result<void> cancel(linux::io_uring_multiplexer& m, async_operation_storage& s)
	{
		return m.cancel_io(s);
	}
};

} // namespace allio

#include <allio/linux/detail/undef.i>
