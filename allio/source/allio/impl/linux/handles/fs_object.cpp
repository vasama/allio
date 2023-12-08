#include <allio/impl/linux/handles/fs_object.hpp>

#include <allio/detail/dynamic_buffer.hpp>
#include <allio/impl/linux/api_string.hpp>
#include <allio/impl/linux/error.hpp>
#include <allio/impl/linux/stat.hpp>
#include <allio/impl/transcode.hpp>
#include <allio/linux/platform.hpp>

#include <vsm/lazy.hpp>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

vsm::result<open_info> linux::make_open_info(
	open_kind const kind,
	open_parameters const& args)
{
	// Linux does not provide file sharing restrictions.
	if (!vsm::all_flags(args.sharing, file_sharing::all))
	{
		return vsm::unexpected(error::unsupported_operation);
	}

	//TODO: Select default based on creation?
	auto default_mode = file_mode::read_write;
	auto maximum_mode = file_mode::read_write;

	if (kind == open_kind::directory)
	{
		default_mode = file_mode::read;
		maximum_mode = file_mode::read;
	}

	open_info info =
	{
		.flags = O_CLOEXEC,
	};

	if (!args.inheritable)
	{
		info.flags |= O_CLOEXEC;
	}

	auto const mode = args.mode.value_or(default_mode);

	if (!vsm::all_flags(maximum_mode, mode))
	{
		return vsm::unexpected(error::invalid_argument);
	}

	switch (mode)
	{
	case file_mode::none:
		break;

	case file_mode::read:
		info.flags |= O_RDONLY;
		break;

	case file_mode::write:
		info.flags |= O_WRONLY;
		break;

	case file_mode::read_write:
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

	return info;
}

vsm::result<unique_fd> linux::open_file(
	int dir_fd,
	char const* const path,
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
		return vsm::unexpected(get_last_error());
	}

	return vsm_lazy(unique_fd(fd));
}


static vsm::result<size_t> _readlink(char const* const path, std::span<char> const buffer)
{
	ssize_t const size = readlink(path, buffer.data(), buffer.size());
	if (size == -1)
	{
		return vsm::unexpected(get_last_error());
	}
	return static_cast<size_t>(size);
}

static vsm::result<std::string_view> read_current_path(char const* const link_path, auto& storage)
{
	std::span<char> buffer;

	while (true)
	{
		//TODO: If a buffer is available, try reading directly into it without lstat.

		// Use lstat to get the size of the linked target path.
		vsm_try(stat, linux::lstat(link_path));

		if (stat.st_size == 0)
		{
			//TODO: Can we deal with this? What's the best error code?
			return vsm::unexpected(error::unknown_failure);
		}

		// This is surely never true, and could not be handled anyway.
		vsm_assert(stat.st_size < std::numeric_limits<ssize_t>::max());

		// Reserve one extra character to correctly interpret the readlink result.
		if (size_t const min_size = stat.st_size + 1; min_size > buffer.size())
		{
			// Reserve whatever extra space is available, up to max.
			vsm_try_assign(buffer, storage.resize(min_size, static_cast<size_t>(-1)));
		}

		// Read the link path directly into the user's buffer.
		vsm_try(path_size, _readlink(link_path, buffer));

		if (path_size < buffer.size())
		{
			// Resize the storage back down to the exact size of the path.
			vsm_try_assign(buffer, storage.resize(path_size));

			return std::string_view(buffer.data(), path_size);
		}
	}
}

vsm::result<size_t> fs_object_t::get_current_path(
	native_type const& h,
	io_parameters_t<fs_object_t, get_current_path_t> const& a)
{
	char link_path[32];
	vsm_verify(snprintf(
		link_path,
		sizeof(link_path),
		"/proc/self/fd/%d",
		unwrap_handle(h.platform_handle)) > 0);

	return a.buffer.visit([&](auto const buffer) -> vsm::result<size_t>
	{
		//TODO: Write directly to the user buffer if encoding matches.
		//      Should the native linux path character type be byte?

		struct storage_wrapper
		{
			dynamic_buffer<char, 1024> storage;

			vsm::result<std::span<char>> resize(size_t const min_size)
			{
				return resize(min_size, min_size);
			}

			vsm::result<std::span<char>> resize(size_t const min_size, size_t const max_size)
			{
				vsm_try(buffer, storage.reserve(min_size));
				return std::span<char>(buffer, std::min(max_size, storage.size()));
			}
		};

		storage_wrapper storage;
		vsm_try(path, read_current_path(link_path, storage));
		return transcode_string(path, buffer);
	});
}
