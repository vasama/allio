#pragma once

#include <allio/detail/handles/thread.hpp>

#include <Windows.h>

namespace allio::win32 {

vsm::result<thread_exit_code> get_thread_exit_code(HANDLE handle);

} // namespace allio::win32
