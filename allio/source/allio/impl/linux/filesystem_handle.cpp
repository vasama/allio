#include <allio/impl/linux/filesystem_handle.hpp>

#include <allio/impl/linux/api_string.hpp>
#include <allio/impl/linux/error.hpp>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::linux;

vsm::result<open_parameters> open_parameters::make(filesystem_handle::open_parameters const& args)
{
	int flags = O_CLOEXEC;
	mode_t mode = 0;

	switch (args.mode)
	{
	case file_mode::none:
		break;

	case file_mode::read:
	case file_mode::read_attributes:
		flags |= O_RDONLY;
		break;

	case file_mode::write:
	case file_mode::write_attributes:
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

vsm::result<unique_fd> linux::create_file(
	filesystem_handle const* const base, input_path_view const path,
	open_parameters const args)
{
	api_string_storage path_storage;
	vsm_try(path_string, make_api_c_string(path_storage, path));

	int const result = openat(
		base != nullptr
			? unwrap_handle(base->get_platform_handle())
			: AT_FDCWD,
		path_string,
		args.flags,
		args.mode);

	if (result == -1)
	{
		return vsm::unexpected(get_last_error());
	}

	return vsm::result<unique_fd>(vsm::result_value, result);
}

vsm::result<unique_fd> linux::create_file(
	filesystem_handle const* const base, input_path_view const path,
	filesystem_handle::open_parameters const& args)
{
	vsm_try(open_args, open_parameters::make(args));
	return create_file(base, path, open_args);
}
