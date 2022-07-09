#include "filesystem_handle.hpp"

#include "api_string.hpp"
#include "error.hpp"

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::linux;

result<open_parameters> open_parameters::make(file_parameters const& args)
{
	int flags = 0;
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
		allio_ERROR(make_error_code(std::errc::not_supported));
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
		allio_ERROR(make_error_code(std::errc::not_supported));
	}

	return open_parameters{ flags, mode };
}

result<unique_fd> linux::create_file(filesystem_handle const* const base, path_view const path, file_parameters const& args)
{
	allio_TRY(open_args, open_parameters::make(args));

	api_string const path_string = path;

	int const result = openat(
		base != nullptr ? unwrap_handle(base->get_platform_handle()) : -1,
		path_string.data(),
		open_args.flags,
		open_args.mode);

	if (result == -1)
	{
		return allio_ERROR(get_last_error_code());
	}

	return { result_value, result };
}
