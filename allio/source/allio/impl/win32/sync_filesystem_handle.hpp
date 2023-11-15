#pragma once

#include <allio/impl/win32/fs_object.hpp>

#include <vsm/utility.hpp>

namespace allio::win32 {

template<std::derived_from<filesystem_handle> Handle>
vsm::result<void> sync_open(io::parameters_with_result<io::filesystem_open> const& args, open_kind const kind)
{
	Handle& h = static_cast<Handle&>(*args.handle);

	if (h)
	{
		return vsm::unexpected(error::handle_is_not_null);
	}

	vsm_try(file, create_file(args.base, args.path, args, kind));

	return initialize_platform_handle(h, vsm_move(file.handle),
		[&](native_platform_handle const handle)
		{
			return platform_handle::native_handle_type
			{
				handle::native_handle_type
				{
					handle::flags::not_null | file.flags,
				},
				handle,
			};
		}
	);
}

} // namespace allio::win32
