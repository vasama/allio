#include <allio/impl/linux/readlink.hpp>

#include <allio/detail/basic_storage.hpp>
#include <allio/impl/error_encoding.hpp>
#include <allio/impl/linux/error.hpp>
#include <allio/impl/linux/stat.hpp>
#include <allio/impl/transcode.hpp>

#include <fcntl.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

static vsm::result<size_t> _readlinkat(
	int const dirfd,
	char const* const path,
	std::span<char> const buffer)
{
	ssize_t const size = ::readlinkat(dirfd, path, buffer.data(), buffer.size());

	if (size == -1)
	{
		return vsm::unexpected(allio_error(get_last_error()));
	}

	return static_cast<size_t>(size);
}

static vsm::result<size_t> _read_link_path(
	int const dirfd,
	char const* const path,
	string_buffer<char> const buffer)
{
	vsm_try(string, buffer.resize(0, static_cast<size_t>(-1)));

	bool is_lstat_available = true;
	auto const resize_buffer = [&]() -> vsm::result<void>
	{
		if (is_lstat_available)
		{
			// Use lstat to get the size of the symbolic link target path.
			vsm_try(stat, linux::fstatat(dirfd, path, AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW));

			static_assert(sizeof(off_t) >= sizeof(size_t));

			if (stat.st_size != 0)
			{
				// Reserve one extra character to correctly interpret the readlink result.
				size_t const min_size = static_cast<size_t>(stat.st_size) + 1;

				if (min_size > string.size())
				{
					return vsm::assign_value(string, buffer.resize(min_size));
				}
			}

			// If st_size is zero or indicates a size smaller than the existing size of the output
			// buffer, something weird is going on and no further lstat calls should be made.
			is_lstat_available = false;
		}

		size_t const min_size = std::max<size_t>(1, string.size() * 3 / 2);

		if (min_size < string.size())
		{
			// Something really weird must be going on if the multiplication overflows.
			return vsm::unexpected(allio_error(error::unknown_failure));
		}

		return vsm::assign_value(string, buffer.resize(min_size, static_cast<size_t>(-1)));
	};

	if (string.empty())
	{
		// If the buffer is empty, it must be grown before calling readlink.
		vsm_try_void(resize_buffer());
	}

	while (true)
	{
		vsm_try(path_size, _readlinkat(dirfd, path, string));

		if (path_size < string.size())
		{
			// Resize the storage back down to the exact size of the path.
			vsm_try_assign(string, buffer.resize(path_size));

			return path_size;
		}

		vsm_try_void(resize_buffer());
	}
}

template<vsm::utf_character Char>
static vsm::result<size_t> _read_link_path(
	int const dirfd,
	char const* const path,
	string_buffer<Char> const buffer)
{
	basic_storage<char, 512> storage;
	vsm_try(path_size, _read_link_path(dirfd, path, storage));
	auto const string = std::string_view(storage.data(), path_size);
	return copy_or_transcode_string(string, buffer);
}

vsm::result<size_t> linux::read_link_path(
	int const dirfd,
	char const* const path,
	string_buffer<char> const buffer)
{
	return _read_link_path(dirfd, path, buffer);
}

vsm::result<size_t> linux::read_link_path(
	int const dirfd,
	char const* const path,
	any_path_buffer const buffer)
{
	return detail::visit_as<char, char16_t, char32_t>(
		buffer.string(),
		[&](auto const buffer)
		{
			return _read_link_path(dirfd, path, buffer);
		});
}
