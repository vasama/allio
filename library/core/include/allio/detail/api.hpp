#pragma once

#include <vsm/platform.h>

#if allio_config_named_module
#	define allio_detail_export export
#else
#	define allio_detail_export
#endif

#if allio_config_dynamic_library
#	if allio_config_dynamic_library_export
#		define allio_detail_api vsm_api_export
#	else
#		define allio_detail_api vsm_api_import
#	endif
#else
#	define allio_detail_api
#endif
