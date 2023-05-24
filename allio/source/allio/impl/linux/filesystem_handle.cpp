#include "filesystem_handle.hpp"

#include "api_string.hpp"
#include "error.hpp"

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::linux;

vsm::result<open_parameters> open_parameters::make(file_parameters const& args)
{
	int flags = O_CLOEXEC;
	mode_t mode = 0;

	switch (args.mode)
	{
	case file_mode::none:
		break;

	case file_mode::read:
	case file_mode::attr_read:
		flags |= O_RDONLY;
		break;

	case file_mode::write:
	case file_mode::attr_write:
		flags |= O_RDWR;
		break;

	default:
		vsm::unexpected(error::unsupported_operation);
	}

	switch (args.creation)
	{
	case file_creation::open_existing:
		break;

	case file_creation::create_only:
		flags |= O_CREAT | O_EXCL;
		break;

	case file_creation::open_or_create:
		flags |= O_CREAT;
		break;

	case file_creation::truncate_existing:
		flags |= O_CREAT | O_TRUNC;
		break;

	default:
		vsm::unexpected(error::unsupported_operation);
	}

	return open_parameters{ flags, mode };
}

vsm::result<unique_fd> linux::create_file(filesystem_handle const* const base, path_view const path, file_parameters const& args)
{
	vsm_try(open_args, open_parameters::make(args));

	vsm_try(path_string, api_string::make(path));

	int const result = openat(
		base != nullptr
			? unwrap_handle(base->get_platform_handle())
			: AT_FDCWD,
		path_string.data(),
		open_args.flags,
		open_args.mode);

	if (result == -1)
	{
		return vsm::unexpected(get_last_error());
	}

	return vsm::result<unique_fd>(vsm::result_value, result);
}
