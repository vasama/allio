#pragma once

#include <allio/impl/linux/filesystem_handle.hpp>

#include <allio/linux/detail/undef.i>

namespace allio {

template<std::derived_from<filesystem_handle> Handle>
struct synchronous_operation_implementation<Handle, io::filesystem_open>
{
	static result<void> execute(io::parameters_with_result<io::filesystem_open> const& args)
	{
		Handle& h = static_cast<Handle&>(*args.handle);

		if (h)
		{
			return allio_ERROR(error::handle_is_not_null);
		}

		allio_TRY(file, linux::create_file(base, path, args));
		return consume_platform_handle(h, { args.handle_flags }, std::move(file));
	}
};

} // namespace allio

#include <allio/linux/detail/undef.i>
