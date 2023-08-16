#pragma once

#include <allio/impl/win32/platform_handle.hpp>
#include <allio/win32/iocp_multiplexer.hpp>

#include <allio/implementation.hpp>

namespace allio {

template<std::derived_from<platform_handle> Handle>
struct multiplexer_handle_implementation<win32::iocp_multiplexer, Handle>
{
	static vsm::result<void> register_handle(win32::iocp_multiplexer& m, Handle const& h)
	{
		return m.register_native_handle(h.get_platform_handle());
	}

	static vsm::result<void> deregister_handle(win32::iocp_multiplexer& m, Handle const& h)
	{
		return m.deregister_native_handle(h.get_platform_handle());
	}
};

template<std::derived_from<platform_handle> Handle>
struct multiplexer_handle_operation_implementation<win32::iocp_multiplexer, Handle, io::close>
{
	static constexpr bool block_synchronous = true;

	using async_operation_storage = basic_async_operation_storage<io::close, deferring_multiplexer::async_operation_storage>;

	static vsm::result<void> start(win32::iocp_multiplexer& m, async_operation_storage& s)
	{
		return m.start(s, [&]() -> vsm::result<void>
		{
			return synchronous<Handle>(s.args);
		});
	}
};

} // namespace allio
