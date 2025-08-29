#include <allio/impl/linux/readlink.hpp>

#include <allio/detail/default_sequence_container.hpp>
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

			static_assert(sizeof(off_t) >= sizeof(size_t));

			// This conditions should never happen, and could not really be handled anyway.
			if (stat.st_size == 0)
			{
				return vsm::unexpected(allio_error(error::unknown_failure));
			}

			// Reserve one extra character to correctly interpret the readlink result.
			vsm_try_assign(string, buffer.resize(static_cast<size_t>(stat.st_size) + 1));
		}

	skip_lstat:
		vsm_try(path_size, _readlinkat(dirfd, path, string));

		if (path_size < string.size())
		{
			// Resize the storage back down to the exact size of the path.
			vsm_try_assign(string, buffer.resize(path_size));

			return path_size;
		}
	}
}

template<vsm::utf_character Char>
static vsm::result<size_t> _read_link_path(
	int const dirfd,
	char const* const path,
	string_buffer<Char> const buffer)
{
	default_sequence_container<char, 512> container;
	vsm_try(path_size, _read_link_path(dirfd, path, container));
	auto const string = std::string_view(container.data(), path_size);
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
