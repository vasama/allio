#include <allio/impl/linux/readlink.hpp>

#include <allio/linux/detail/undef.i>

using namespace allio;
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

static vsm::result<std::string_view> _read_link_path(
	int const dirfd,
	char const* const path,
	string_buffer<char> const buffer)
{
	vsm_try(string, buffer.resize(0, static_cast<size_t>(-1)));

	if (!string.empty())
	{
		// If the buffer is non-empty, try readlink before calling lstat.
		goto skip_lstat;
	}

	while (true)
	{
		// Query the path size using lstat and resize the buffer.
		{
			// Use lstat to get the size of the symbolic link target path.
			vsm_try(stat, linux::fstatat(dirfd, path, AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW));

			// These conditions should never happen, and could not really be handled anyway.
			if (stat.st_size == 0 || stat.st_size >= std::numeric_limits<size_t>::max())
			{
				return vsm::unexpected(allio_error(error::unknown_failure));
			}

			// Reserve one extra character to correctly interpret the readlink result.
			vsm_try_assign(string, buffer.resize(static_cast<size_t>(stat.st_size) + 1));
		}

	skip_lstat:
		vsm_try(path_size, _readlinkat(dirfd, path, buffer));

		if (path_size < buffer.size())
		{
			// Resize the storage back down to the exact size of the path.
			vsm_try_assign(string, buffer.resize(path_size));

			return std::string_view(string.data(), path_size);
		}
	}
}

template<typename Char>
static vsm::result<std::string_view> _read_link_path(
	int const dirfd,
	char const* const path,
	string_buffer<Char> const buffer);

vsm::result<std::string_view> linux::read_link_path(
	int const dirfd,
	char const* const path,
	any_path_buffer const buffer)
{
	
}
