#include <allio/impl/linux/filesystem.hpp>

#include <allio/impl/linux/api_string.hpp>
#include <allio/impl/linux/error.hpp>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

vsm::result<unique_handle> linux::create_file(
	int dir_fd,
	char const *const path,
	int const flags,
	mode_t const mode)
{
	if (dir_fd == -1)
	{
		dir_fd = AT_FDCWD;
	}

	int const fd = openat(
		dir_fd,
		path,
		flags,
		mode);

	if (fd == -1)
	{
		return vsm::unexpected(allio_error(get_last_error()));
	}

	return vsm::result<unique_handle>(vsm::result_value, fd);
}

vsm::result<unique_handle> linux::create_file(
	int const dir_fd,
	char const *const path,
	platform_open_options const &options)
{
	return create_file(dir_fd, path, options.flags, options.mode);
}

vsm::result<unique_handle> linux::reopen_file(
	int const fd,
	int const flags,
	mode_t const mode)
{
	if (flags & O_DIRECTORY)
	{
		return linux::create_file(
			fd,
			".",
			flags,
			mode);
	}

	char link_path[32];
	vsm_verify(snprintf(link_path, sizeof(link_path), "/proc/self/fd/%d", fd) > 0);

	int const new_fd = open(
		link_path,
		flags,
		mode);

	if (new_fd == -1)
	{
		return vsm::unexpected(allio_error(get_last_error()));
	}

	return vsm::result<unique_handle>(vsm::result_value, new_fd);
}

vsm::result<unique_handle> linux::reopen_file(
	int const fd,
	platform_open_options const &options)
{
	return reopen_file(fd, options.flags, options.mode);
}

vsm::result<unique_handle> linux::open_anchor_file(
	int const dir_fd,
	char const *const path,
	bool const may_open_leaf)
{
	platform_open_options options =
	{
		.flags = O_CLOEXEC,
	};

	if (!may_open_leaf)
	{
		options.flags |= O_PATH | O_DIRECTORY;
	}

	return create_file(dir_fd, path, options);
}

vsm::result<unique_handle> detail::open_anchor_file(
	int const dir_fd,
	any_path_view const path,
	bool const may_open_leaf)
{
	api_string_storage storage;
	vsm_try(path_c_str, make_api_c_string(storage, path.string()));
	return linux::open_anchor_file(dir_fd, path_c_str, may_open_leaf);
}

vsm::result<unique_handle> detail::open_file(
	int const dir_fd,
	any_path_view const path,
	platform_open_options const &options)
{
	api_string_storage storage;
	vsm_try(path_c_str, make_api_c_string(storage, path.string()));
	return linux::create_file(dir_fd, path_c_str, options);
}

vsm::result<unique_handle> detail::open_anonymous_file(
	int const dir_fd,
	platform_open_options const &options)
{
	// Anonymous files can only be opened as create_only.
	vsm_assert(vsm::all_flags(options.flags, O_CREAT | O_EXCL));

	platform_open_options anonymous_options = options;
	anonymous_options.flags &= ~(O_CREAT | O_EXCL);
	anonymous_options.flags |= O_TMPFILE;

	return linux::create_file(dir_fd, ".", anonymous_options);
}

vsm::result<void> linux::link_file_at(
	int const fd,
	int dir_fd,
	char const *const path,
	bool const replace_existing_file)
{
	if (replace_existing_file)
	{
		return vsm::unexpected(error::unsupported_operation);
	}

	if (dir_fd == -1)
	{
		dir_fd = AT_FDCWD;
	}

	int const r = linkat(fd, "", dir_fd, path, AT_EMPTY_PATH);

	if (r == -1)
	{
		return vsm::unexpected(allio_error(get_last_error()));
	}

	return {};
}

vsm::result<void> detail::link_file_at(
	int const fd,
	int const dir_fd,
	any_path_view const path,
	bool const replace_existing_file)
{
	if (replace_existing_file)
	{
		return vsm::unexpected(allio_error(error::unsupported_operation));
	}

	api_string_storage storage;
	vsm_try(path_c_str, make_api_c_string(storage, path.string()));
	return linux::link_file_at(fd, dir_fd, path_c_str, replace_existing_file);
}
