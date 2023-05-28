#pragma once

#include <allio/impl/linux/filesystem_handle.hpp>

#include <vsm/utility.hpp>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

template<std::derived_from<filesystem_handle> Handle>
vsm::result<void> sync_open(io::parameters_with_result<io::filesystem_open> const& args)
{
	Handle& h = static_cast<Handle&>(*args.handle);

	if (h)
	{
		return vsm::unexpected(error::handle_is_not_null);
	}

	vsm_try(file, create_file(args.base, args.path, args));
	return initialize_platform_handle(h, vsm_move(file), [&](auto const handle)
	{
		return platform_handle::native_handle_type
		{
			handle::native_handle_type
			{
				handle::flags::not_null,
			},
			handle,
		};
	});
}

} // namespace allio::linux

#if 0
namespace allio {

template<std::derived_from<filesystem_handle> Handle>
struct synchronous_operation_implementation<Handle, io::filesystem_open>
{
	static vsm::result<void> execute(io::parameters_with_result<io::filesystem_open> const& args)
	{
		Handle& h = static_cast<Handle&>(*args.handle);

		if (h)
		{
			return vsm::unexpected(error::handle_is_not_null);
		}

		vsm_try(file, linux::create_file(args.base, args.path, args));
		return consume_platform_handle(h, { args.handle_flags }, vsm_move(file));
	}
};

} // namespace allio
#endif

#include <allio/linux/detail/undef.i>
