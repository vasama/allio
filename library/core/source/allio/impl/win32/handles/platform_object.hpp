#pragma once

#include <allio/impl/handles/platform_object.hpp>
#include <allio/win32/handles/platform_object.hpp>

#include <allio/detail/unique_handle.hpp>

#include <vsm/utility.hpp>

#include <Windows.h>

namespace allio::win32 {

bool verify_object_type(HANDLE handle, std::wstring_view type);
vsm::result<ACCESS_MASK> get_handle_access(HANDLE handle);
vsm::result<detail::unique_handle> duplicate_handle(HANDLE handle);
detail::handle_flags set_file_completion_notification_modes(HANDLE handle);

} // namespace allio::win32
