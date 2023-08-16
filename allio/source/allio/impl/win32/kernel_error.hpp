#pragma once

//TODO: Rename nt_errot to kernel_error.
#include <allio/win32/nt_error.hpp>

namespace allio::win32 {

vsm::result<void> check_status(NTSTATUS const status)
{
	
}

} // namespace allio::win32
