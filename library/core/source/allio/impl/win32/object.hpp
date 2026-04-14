#pragma once

#include <allio/detail/unique_handle.hpp>
#include <allio/impl/win32/kernel.hpp>

namespace allio::win32 {

vsm::result<ACCESS_MASK> get_handle_access(HANDLE handle);

vsm::result<detail::unique_handle> duplicate_handle(HANDLE handle);

} // namespace allio::win32
