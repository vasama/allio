#pragma once

#include <allio/impl/win32/filesystem_handle.hpp>

namespace allio::win32 {

template<std::derived_from<filesystem_handle> Handle>
result<void> sync_open(io::parameters_with_result<io::filesystem_open> const& args, open_kind const kind)
{
	Handle& h = static_cast<Handle&>(*args.handle);

	if (h)
	{
		return allio_ERROR(error::handle_is_not_null);
	}

	allio_TRY(file, create_file(args.base, args.path, args, kind));
	return initialize_platform_handle(h, std::move(file.handle), [&](auto const handle)
	{
		return platform_handle::native_handle_type
		{
			handle::native_handle_type
			{
				file.flags | handle::flags::not_null,
			},
			handle,
		};
	});
}

} // namespace allio::win32

#if 0
template<std::derived_from<filesystem_handle> Handle>
struct synchronous_operation_implementation<Handle, io::filesystem_open>
{
	static result<void> execute(io::parameters_with_result<io::filesystem_open> const& args)
	{
		using namespace win32;

		Handle& h = static_cast<Handle&>(*args.handle);

		if (h)
		{
			return allio_ERROR(error::handle_is_not_null);
		}

		allio_TRY(file, create_file(args.base, args.path, args, win32::handle_open_kind<Handle>::value));
		return initialize_platform_handle(h, std::move(file.handle), [&](auto const handle)
		{
			return platform_handle::native_handle_type
			{
				handle::native_handle_type
				{
					file.flags,
				},
				handle,
			};
		});
	}
};

template<std::derived_from<filesystem_handle> Handle, typename Char>
struct synchronous_operation_implementation<Handle, io::get_current_path<Char>>
{
	static result<void> execute(io::parameters_with_result<io::get_current_path<Char>> const& args)
	{
		return allio_ERROR(error::unsupported_operation);
	}
};

template<std::derived_from<filesystem_handle> Handle, typename Char>
struct synchronous_operation_implementation<Handle, io::copy_current_path<Char>>
{
	static result<void> execute(io::parameters_with_result<io::copy_current_path<Char>> const& args)
	{
		return allio_ERROR(error::unsupported_operation);
	}
};
#endif
