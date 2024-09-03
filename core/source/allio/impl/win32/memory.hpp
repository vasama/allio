#pragma once

#include <allio/detail/memory.hpp>

#include <Windows.h>

namespace allio::win32 {

vsm::result<ULONG> get_page_protection(detail::protection protection);

} // namespace allio::win32
