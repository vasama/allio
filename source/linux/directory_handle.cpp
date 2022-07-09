#include <allio/directory_handle.hpp>

#include "error.hpp"

#include <dirent.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::linux;

result<directory_entry<path>> detail::directory_stream_base::next<>()
{1
	DIR* const dirp = unwrap_handle<DIR*>(get_platform_handle());

	errno = 0;
	if (dirent* const d = readdir(dirp))
	{
	}
	else if (errno)
	{
		return allio_ERROR(get_last_error_code());
	}
	else
	{
		return std::nullopt;
	}
}

result<directory_entry<path_view>> detail::directory_stream_base::next<>(std::span<char> const buffer)
{
	
}

result<void> detail::directory_stream_base::do_close()
{
	allio_TRY(native_handle, platform_handle::release_native_handle());
	if (closedir(unwrap_handle<DIR*>(native_handle)) == -1)
	{
		return allio_ERROR(get_last_error_code());
	}
	return {};
}
