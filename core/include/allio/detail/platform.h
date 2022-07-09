#pragma once

#include <vsm/platform.h>

#if vsm_os_win32
#	define allio_detail_platform_wide 1
#endif

#ifndef allio_detail_platform_wide
#	define allio_detail_platform_wide 0
#endif

#if allio_detail_platform_wide
#	define allio_detail_platform_char wchar_t
#else
#	define allio_detail_platform_char char
#endif
