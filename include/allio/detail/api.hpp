#pragma once

#if _WIN32
#	define allio_API_EXPORT declspec(dllexport)
#	define allio_API_IMPORT declspec(dllimport)
#else
#	define allio_API_EXPORT
#	define allio_API_IMPORT
#endif

#if allio_CONFIG_DYNAMIC_LIBRARY
#	if allio_CONFIG_DYNAMIC_LIBRARY_EXPORT
#		define allio_API allio_API_EXPORT
#	else
#		define allio_API allio_API_IMPORT
#	endif
#else
#	define allio_API
#endif
