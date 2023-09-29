#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// @brief ABI stable structure representing some foreign pollable object.
///        Refer to the opaque handle provider for what action to take upon poll completion.
///        * Posix: The handle may be polled using POSIX poll or equivalent.
///        * Windows: The handle may be polled using Windows WaitForSingleObject or equivalent.
struct allio_opaque_handle_v1
{
	/// @brief Platform specific pollable handle value.
	///        * Posix: file descriptor
	///        * Windows: HANDLE
	uintptr_t handle;

	/// @brief Platform specific information for how to poll the specified handle.
	///        * Posix: Bitmask of POLL* flags.
	///        * Windows: not used.
	uintptr_t information;
};

#ifdef __cplusplus
} // extern "C"
#endif
