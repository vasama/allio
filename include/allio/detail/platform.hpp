#pragma once

#if defined(_WIN32)
#	define allio_detail_WIN32 1
#	define allio_detail_PLATFORM win32
#elif defined(__linux__)
#	define allio_detail_LINUX 1
#	define allio_detail_PLATFORM linux
#else
#	error Unsupported platform
#endif

#ifdef _MSC_VER
#	define allio_detail_ALLOCA(size) _alloca(size)
#else
#	define allio_detail_ALLOCA(size) __builtin_alloca(size)
#endif

#define allio_detail_LIKELY(...) __VA_ARGS__
#define allio_detail_UNLIKELY(...) __VA_ARGS__
