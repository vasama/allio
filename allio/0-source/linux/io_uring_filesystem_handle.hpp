#pragma once

#include <allio/impl/linux/filesystem_handle.hpp>
#include <allio/impl/linux/io_uring_platform_handle.hpp>

#include <allio/impl/linux/api_string.hpp>

#include <allio/linux/detail/undef.i>

namespace allio {

template<std::derived_from<filesystem_handle> Handle>
struct multiplexer_handle_operation_implementation<linux::io_uring_multiplexer, Handle, io::filesystem_open>
{
	struct async_operation_storage : basic_async_operation_storage<io::filesystem_open, linux::io_uring_multiplexer::async_operation_storage>
	{
		using basic_async_operation_storage::basic_async_operation_storage;

		linux::api_string_storage path_storage;
		linux::open_parameters open_args;

		linux::io_uring_multiplexer::timeout timeout;

		void io_completed(linux::io_uring_multiplexer& m, linux::io_uring_multiplexer::io_data_ref, int const result) override
		{
			Handle& h = static_cast<Handle&>(*args.handle);

			vsm_assert(!h &&
				"filesystem_handle was initialized during asynchronous open via io uring.");

			if (result >= 0)
			{
				vsm_verify(h.set_native_handle(
					filesystem_handle::native_handle_type
					{
						platform_handle::native_handle_type
						{
							handle::native_handle_type
							{
								handle::flags::not_null,
							},
							linux::wrap_handle(result),
						}
					}
				));

				set_result(std::error_code());
			}
			else
			{
				set_result(static_cast<linux::system_error>(-result));
			}

			m.post(*this, async_operation_status::concluded);
		}
	};

	static vsm::result<void> start(linux::io_uring_multiplexer& m, async_operation_storage& s)
	{
		return m.start(s, [&]() -> async_result<void>
		{
			filesystem_handle& h = *s.args.handle;

			if (h)
			{
				return vsm::unexpected(error::handle_is_not_null);
			}

			if (!s.args.multiplexable)
			{
				return vsm::unexpected(error::invalid_argument);
			}

			vsm_try(path_string, linux::make_api_c_string(s.path_storage, s.args.path));
			vsm_try_assign(s.open_args, linux::open_parameters::make(s.args));

			return m.record_io([&](linux::io_uring_multiplexer::submission_context& context) -> async_result<void>
			{
				if (!s.args.deadline.is_trivial())
				{
					vsm_try_void(context.push_linked_timeout(s.timeout.set(s.args.deadline)));
				}

				return context.push(s, [&](io_uring_sqe& sqe)
				{
					sqe.opcode = IORING_OP_OPENAT;
					sqe.fd = s.args.base != nullptr
						? linux::unwrap_handle(s.args.base->get_platform_handle())
						: -1;
					sqe.addr = reinterpret_cast<uintptr_t>(path_string);
					sqe.open_flags = s.open_args.flags;
					sqe.len = s.open_args.mode;
				});
			});
		});
	}

	static vsm::result<void> cancel(linux::io_uring_multiplexer& m, async_operation_storage& s)
	{
		return m.cancel_io(s);
	}
};

} // namespace allio

#include <allio/linux/detail/undef.i>
