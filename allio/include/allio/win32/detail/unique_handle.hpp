#pragma once

#include <allio/win32/platform.hpp>

#include <vsm/result.hpp>
#include <vsm/unique_resource.hpp>

namespace allio::win32 {

vsm::result<void> close_handle(HANDLE handle);

struct handle_deleter
{
	void operator()(HANDLE const handle) const
	{
		vsm_verify(close_handle(handle));
	}
};
using unique_handle = vsm::unique_resource<HANDLE, handle_deleter, static_cast<HANDLE>(NULL)>;

} // namespace allio::win32
