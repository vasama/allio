#pragma once

#include <allio/win32/detail/unique_handle.hpp>
#include <allio/win32/handles/platform_object.hpp>
#include <allio/win32/platform.hpp>

#include <vsm/utility.hpp>

#include <win32.h>

namespace allio::win32 {

struct unique_handle_with_flags
{
	detail::unique_handle handle;
	detail::handle_flags flags;
};

vsm::result<ACCESS_MASK> get_handle_access(HANDLE handle);
vsm::result<detail::unique_handle> duplicate_handle(HANDLE handle);
detail::handle_flags set_file_completion_notification_modes(HANDLE handle);

} // namespace allio::win32
