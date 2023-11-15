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
	handle_flags flags;
};

handle_flags set_file_completion_notification_modes(HANDLE handle);

vsm::result<detail::unique_handle> duplicate_handle(HANDLE handle);

} // namespace allio::win32
