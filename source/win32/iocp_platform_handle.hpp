#pragma once

#include "platform_handle.hpp"
#include <allio/win32/iocp_multiplexer.hpp>

#include <allio/static_multiplexer_handle_relation_provider.hpp>

namespace allio {

template<std::derived_from<platform_handle> Handle>
struct allio::multiplexer_handle_implementation<win32::iocp_multiplexer, Handle>
{
	static result<void*> register_handle(win32::iocp_multiplexer& m, Handle const& h)
	{
		allio_TRY(flags, m.register_native_handle(h.get_platform_handle()));
		return win32::iocp_multiplexer::make_data(flags);
	}

	static result<void> deregister_handle(win32::iocp_multiplexer& m, Handle const& h)
	{
		return m.deregister_native_handle(h.get_platform_handle());
	}
};

template<std::derived_from<platform_handle> Handle>
struct multiplexer_handle_operation_implementation<win32::iocp_multiplexer, Handle, io::close>
{
	using async_operation_storage = win32::iocp_multiplexer::basic_async_operation_storage<io::close>;

	static result<void> start(win32::iocp_multiplexer& m, async_operation_storage& s)
	{
		allio_TRY(handle, static_cast<Handle*>(s.handle)->release_native_handle());
		allio_VERIFY(win32::close_handle(win32::unwrap_handle(handle.handle)));
		m.post_synchronous_completion(s);
		return {};
	}
};

} // namespace allio
