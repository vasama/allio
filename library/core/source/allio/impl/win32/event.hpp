#pragma once

#include <allio/detail/unique_handle.hpp>
#include <allio/win32/detail/win32_fwd.hpp>

#include <vsm/result.hpp>

namespace allio::win32 {

vsm::result<detail::unique_handle> create_event(
	bool auto_reset = true,
	bool initially_signaled = false);

vsm::result<void> signal_event(detail::HANDLE event);

} // namespace allio::win32
