#pragma once

#include "filesystem_handle.hpp"
#include "io_uring_platform_handle.hpp"

#include "api_string.hpp"

#include <allio/linux/detail/undef.i>

namespace allio {

template<std::derived_from<platform_handle> Handle>
struct allio::multiplexer_handle_operation_implementation<linux::io_uring_multiplexer, Handle, io::open_file>
{
	struct async_operation_storage : linux::io_uring_multiplexer::basic_async_operation_storage<io::open_file>
	{
		using basic_async_operation_storage::basic_async_operation_storage;

		linux::api_string path_string;
		linux::open_parameters open_args;
	};

	static result<void> start(linux::io_uring_multiplexer& m, async_operation_storage& s)
	{
		allio_ASSERT((s.args.handle_flags & flags::multiplexable) != flags::none);

		allio_TRYA(s.open_args, linux::open_parameters::make(s.args));

		return m.push(s, +[](async_operation_storage& s, io_uring_sqe& sqe)
		{
			sqe.opcode = IORING_OP_OPENAT;
			sqe.fd = s.base != nullptr ? linux::unwrap_handle(s.base->get_platform_handle()) : -1;
			sqe.addr = reinterpret_cast<uintptr_t>(s.path_string.set_string(s.path));
			sqe.open_flags = s.open_args.flags;
			sqe.len = s.open_args.mode;

			s.capture_result([](async_operation_storage& s, int const result) -> allio::result<void>
			{
				allio_ASSERT(!*s.handle);
				return linux::consume_platform_handle(
					static_cast<Handle&>(*s.handle), { s.args.handle_flags }, linux::unique_fd(result));
			});
		});
	}

	static result<void> cancel(linux::io_uring_multiplexer& m, async_operation_storage& s)
	{
		return m.cancel(s);
	}
};

} // namespace allio

#include <allio/linux/detail/undef.i>
