#pragma once

#include <allio/impl/error_encoding.hpp>
#include <allio/impl/linux/error.hpp>

#include <sys/stat.h>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

inline vsm::result<struct stat> stat(char const* const path)
{
	struct stat result;
	if (::stat(path, &result) == -1)
	{
		return vsm::unexpected(allio_error(get_last_error()));
	}
	return result;
}

inline vsm::result<struct stat> fstat(int const fd)
{
	struct stat result;
	if (::fstat(fd, &result) == -1)
	{
		return vsm::unexpected(allio_error(get_last_error()));
	}
	return result;
}

inline vsm::result<struct stat> lstat(char const* const path)
{
	struct stat result;
	if (::lstat(path, &result) == -1)
	{
		return vsm::unexpected(allio_error(get_last_error()));
	}
	return result;
}

inline vsm::result<struct stat> fstatat(int const dirfd, char const* const path, int const flags)
{
	struct stat result;
	if (::fstatat(dirfd, path, &result, flags))
	{
		return vsm::unexpected(allio_error(get_last_error()));
	}
	return result;
}

} // namespace allio::linux

#include <allio/linux/detail/undef.i>
