#include <allio/impl/linux/filesystem_handle.hpp>

#include <allio/impl/linux/api_string.hpp>
#include <allio/impl/linux/error.hpp>
#include <allio/impl/linux/stat.hpp>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

vsm::result<open_parameters> open_parameters::make(filesystem_handle::open_parameters const& args)
{
	open_parameters info =
	{
		.flags = O_CLOEXEC,
	};

	switch (args.mode)
	{
	case file_mode::none:
		break;

	case file_mode::read:
	case file_mode::read_attributes:
		info.flags |= O_RDONLY;
		break;

	case file_mode::write:
	case file_mode::write_attributes:
		info.flags |= O_RDWR;
		break;

	default:
		vsm::unexpected(error::unsupported_operation);
	}

	switch (args.creation)
	{
	case file_creation::open_existing:
		break;

	case file_creation::create_only:
		info.flags |= O_CREAT | O_EXCL;
		break;

	case file_creation::open_or_create:
		info.flags |= O_CREAT;
		break;

	case file_creation::truncate_existing:
		info.flags |= O_CREAT | O_TRUNC;
		break;

	default:
		vsm::unexpected(error::unsupported_operation);
	}

	// Linux does not provide file sharing restrictions.
	if (!vsm::all_flags(args.sharing, file_sharing::all))
	{
		return vsm::unexpected(error::unsupported_operation);
	}

	return info;
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


namespace allio::linux {

static vsm::result<size_t> readlink(char const* const path, std::span<char> const buffer)
{
	ssize_t const size = ::readlink(path, buffer.data(), buffer.size());
	if (size == -1)
	{
		return vsm::unexpected(get_last_error());
	}
	return static_cast<size_t>(size);
}

} // namespace allio::linux

vsm::result<void> filesystem_handle::sync_impl(io::parameters_with_result<io::get_current_path> const& args)
{
	filesystem_handle const& h = *args.handle;

	if (!h)
	{
		return vsm::unexpected(error::handle_is_null);
	}

	if (!args.output.is_utf8())
	{
		return vsm::unexpected(error::unsupported_encoding);
	}

	int const fd = unwrap_handle(h.get_platform_handle());

	char link_path[32];
	vsm_verify(snprintf(link_path, sizeof(link_path), "/proc/self/fd/%d", fd) > 0);

	// Use lstat to get the size of the linked target path.
	vsm_try(stat, linux::lstat(link_path));

	if (stat.st_size == 0)
	{
		//TODO: Can we deal with this? What's the best error code?
		return vsm::unexpected(error::unsupported_operation);
	}

	//TODO: Can the size change between the call to lstat and the call to readlink,
	//      given that we're holding the file descriptor in question?

	output_string_ref const output = args.output;

	// Try to resize the user's output buffer to the required size. 
	vsm_try(buffer, output.resize_utf8(stat.st_size + output.is_c_string()));

	// Read the link path into the user's output buffer.
	vsm_try(size, linux::readlink(link_path, buffer));

	//TODO: Will deleted files prepend or append "(deleted)" and will that be reflected in st_size?

	if (output.is_c_string())
	{
		buffer[size] = '\0';
	}

	*args.result = size;

	return {};
}
