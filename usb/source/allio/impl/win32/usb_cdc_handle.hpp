#pragma once

#include <allio/usb/usb_cdc_handle.hpp>

#include <winusb.h>

namespace allio::win32 {

inline native_platform_handle wrap_usb(WINUSB_INTERFACE_HANDLE const handle)
{
	return wrap_handle<WINUSB_INTERFACE_HANDLE>(handle);
}

inline WINUSB_INTERFACE_HANDLE unwrap_usb(native_platform_handle const handle)
{
	return unwrap_handle<WINUSB_INTERFACE_HANDLE>(handle);
}

} // namespace allio::win32
