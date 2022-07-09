#pragma once

#include "filesystem_handle.hpp"
#include "iocp_platform_handle.hpp"

namespace allio {

template<std::derived_from<platform_handle> Handle>
struct multiplexer_handle_operation_implementation<win32::iocp_multiplexer, Handle, io::open_file>
{
	using async_operation_storage = win32::iocp_multiplexer::basic_async_operation_storage<io::open_file>;

	static constexpr bool synchronous = true;

	static result<void> start(win32::iocp_multiplexer& m, async_operation_storage& s)
	{
		allio_ASSERT((s.args.handle_flags & flags::multiplexable) != flags::none);
		allio_TRY(file, win32::create_file(s.base, s.path, s.args));
		allio_TRYV(win32::consume_platform_handle(static_cast<Handle&>(*s.handle), { s.args.handle_flags }, std::move(file)));
		m.post_synchronous_completion(s);
		return {};
	}
};

} // namespace allio
