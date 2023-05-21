#pragma once

#include <allio/impl/linux/filesystem_handle.hpp>
#include <allio/impl/linux/io_uring_platform_handle.hpp>

#include <allio/impl/linux/api_string.hpp>

#include <allio/linux/detail/undef.i>

namespace allio {

template<std::derived_from<filesystem_handle> Handle>
struct allio::multiplexer_handle_operation_implementation<linux::io_uring_multiplexer, Handle, io::filesystem_open>
{
	struct async_operation_storage : linux::io_uring_multiplexer::basic_async_operation_storage<io::filesystem_open>
	{
		using basic_async_operation_storage::basic_async_operation_storage;

		linux::api_string path_string;
		linux::open_parameters open_args;
	};

	static vsm::result<void> start(linux::io_uring_multiplexer& m, async_operation_storage& s)
	{
		vsm_assert((s.args.handle_flags & flags::multiplexable) != flags::none);

		vsm_try_void(s.path_string.set(s.args.path));
		vsm_try_assign(s.open_args, linux::open_parameters::make(s.args));

		return m.push(s, +[](async_operation_storage& s, io_uring_sqe& sqe)
		{
			sqe.opcode = IORING_OP_OPENAT;
			sqe.fd = s.base != nullptr ? linux::unwrap_handle(s.base->get_platform_handle()) : -1;
			sqe.addr = reinterpret_cast<uintptr_t>(s.path_string.c_string());
			sqe.open_flags = s.open_args.flags;
			sqe.len = s.open_args.mode;

			s.capture_result([](async_operation_storage& s, int const result) -> vsm::result<void>
			{
				vsm_assert(!*s.handle);
				return linux::consume_platform_handle(
					static_cast<Handle&>(*s.handle), { s.args.handle_flags }, linux::unique_fd(result));
			});
		});
	}

	static vsm::result<void> cancel(linux::io_uring_multiplexer& m, async_operation_storage& s)
	{
		return m.cancel(s);
	}
};

} // namespace allio

#include <allio/linux/detail/undef.i>
