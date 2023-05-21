#include <allio/win32/detail/unique_handle.hpp>

#include <allio/impl/win32/error.hpp>

#include <win32.h>

using namespace allio;
using namespace allio::win32;

vsm::result<void> win32::close_handle(HANDLE const handle)
{
	if (!CloseHandle(handle))
	{
		return std::unexpected(get_last_error());
	}
	return {};
}
