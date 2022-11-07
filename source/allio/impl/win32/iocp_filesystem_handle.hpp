#pragma once

#include <allio/impl/win32/filesystem_handle.hpp>

#include <allio/implementation.hpp>

#include <allio/impl/win32/iocp_platform_handle.hpp>

namespace allio {

template<std::derived_from<platform_handle> Handle>
struct multiplexer_handle_operation_implementation<win32::iocp_multiplexer, Handle, io::filesystem_open>
{
	static constexpr bool block_synchronous = true;

	using async_operation_storage = basic_async_operation_storage<io::filesystem_open, deferring_multiplexer::async_operation_storage>;

	static result<void> start(win32::iocp_multiplexer& m, async_operation_storage& s)
	{
		return m.start(s, [&]() -> result<void>
		{
			if (!s.args.multiplexable)
			{
				return allio_ERROR(error::invalid_argument);
			}

			return synchronous<Handle>(s.args);
		});
	}
};

} // namespace allio
