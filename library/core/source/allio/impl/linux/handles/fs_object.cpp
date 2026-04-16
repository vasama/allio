#include <allio/impl/linux/handles/fs_object.hpp>

#include <allio/detail/dynamic_buffer.hpp>
#include <allio/impl/linux/api_string.hpp>
#include <allio/impl/linux/error.hpp>
#include <allio/impl/linux/stat.hpp>
#include <allio/impl/transcode.hpp>

#include <vsm/lazy.hpp>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

file_sharing detail::default_file_sharing(open_options const options)
{
	return file_sharing::all;
}

vsm::result<platform_open_options> detail::make_platform_open_options(
	generic_open_options const& options)
{
	platform_open_options r = {};

	// Linux does not provide file sharing restrictions.
	if (!vsm::all_flags(options.sharing, file_sharing::all))
	{
		return vsm::unexpected(allio_error(error::unsupported_operation));
	}

	auto maximum_mode = file_mode::read_write;

	switch (options.kind)
	{
	case open_kind::path:
		r.flags |= O_PATH;
		maximum_mode = file_mode::none;
		break;

	case open_kind::file:
		break;

	case open_kind::directory:
		r.flags |= O_DIRECTORY;
		maximum_mode = file_mode::read;
		break;
	}

	if (r.flags & (O_PATH | O_DIRECTORY))
	{
		if (options.opening != file_opening::open_existing)
		{
			return vsm::unexpected(allio_error(error::invalid_argument));
		}
	}

	if (!vsm::all_flags(maximum_mode, options.mode))
	{
		return vsm::unexpected(allio_error(error::invalid_argument));
	}

	if (vsm::no_flags(options.flags, io_flags::create_inheritable))
	{
		r.flags |= O_CLOEXEC;
	}

	switch (options.mode)
	{
	case file_mode::none:
		break;

	case file_mode::read:
		r.flags |= O_RDONLY;
		break;

	case file_mode::write:
		r.flags |= O_WRONLY;
		break;

	case file_mode::read_write:
		r.flags |= O_RDWR;
		break;

	default:
		return vsm::unexpected(allio_error(error::unsupported_operation));
	}

	switch (options.opening)
	{
	case file_opening::open_existing:
		break;

	case file_opening::create_only:
		r.flags |= O_CREAT | O_EXCL;
		break;

	case file_opening::open_or_create:
		r.flags |= O_CREAT;
		break;

	case file_opening::truncate_existing:
		r.flags |= O_CREAT | O_TRUNC;
		break;

	default:
		return vsm::unexpected(allio_error(error::unsupported_operation));
	}

	if (r.flags & O_CREAT)
	{
		// Apply read/write permissions for owner, group and other by default.
		r.mode = 0b110'110'110;
	}

	return r;
}

vsm::result<handle_flags> detail::configure_file(
	platform_handle_type const handle,
	platform_open_options const& options)
{
	return handle_flags::none;
}

static vsm::result<size_t> _readlink(char const* const path, std::span<char> const buffer)
{
	ssize_t const size = readlink(path, buffer.data(), buffer.size());
	if (size == -1)
	{
		return vsm::unexpected(allio_error(get_last_error()));
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
			return vsm::unexpected(allio_error(error::unknown_failure));
		}

		// This is surely never the case, and could not be handled anyway.
		vsm_assert(stat.st_size <= std::numeric_limits<ssize_t>::max());

		// Reserve one extra character to correctly interpret the readlink result.
		if (size_t const min_size = static_cast<size_t>(stat.st_size) + 1; min_size > buffer.size())
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
	native_handle<fs_object_t> const& h,
	io_parameters_t<fs_object_t, get_current_path_t> const& a)
{
	char link_path[32];
	vsm_verify(snprintf(
		link_path,
		sizeof(link_path),
		"/proc/self/fd/%d",
		unwrap_handle(h.platform_handle)) > 0);

	return a.buffer.string().visit([&](auto const buffer) -> vsm::result<size_t>
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
