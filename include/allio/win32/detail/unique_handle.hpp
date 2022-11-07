#pragma once

#include <allio/detail/unique_resource.hpp>
#include <allio/result.hpp>
#include <allio/win32/platform.hpp>

namespace allio::win32 {

result<void> close_handle(HANDLE handle);

struct handle_deleter
{
	void operator()(HANDLE const handle) const
	{
		allio_VERIFY(close_handle(handle));
	}
};
using unique_handle = detail::unique_resource<HANDLE, handle_deleter, static_cast<HANDLE>(NULL)>;

} // namespace allio::win32
