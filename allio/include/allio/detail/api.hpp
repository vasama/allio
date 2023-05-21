#pragma once

#include <vsm/platform.h>

#if vsm_config_dynamic_library
#	if vsm_config_dynamic_library_export
#		define allio_detail_api vsm_api_export
#	else
#		define allio_detail_api vsm_api_import
#	endif
#else
#	define allio_detail_api
#endif
