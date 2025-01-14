#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum
{
	allio_abi_v1 = 1,
};

enum allio_abi_result
{
	/// @brief The operation completed successfully.
	allio_abi_result_success = 0,

	/// @brief The operation could not be completed. Retry by polling the object again.
	allio_abi_result_try_again,
}
typedef allio_abi_result;

/// @brief ABI stable native object type for asynchronous polling across ABI boundaries such as
///        dynamically linked library interfaces. Instead of exposing allio C++ types in your
///        library interface, return an a pointer to allio_abi_object and wrap it in a
///        @ref opaque_handle before returning it to the user of your library.
struct allio_abi_object
{
	uint32_t version;

	/// @brief Platform specific pollable handle value.
	///        * Posix:   file descriptor
	///        * Windows: HANDLE
	uintptr_t handle_value;

	/// @brief Platform specific information describing the object and how to poll it.
	///        * Posix:   Bitmask of POLL* flags.
	///        * Windows: not used.
	uintptr_t object_flags;

	/// @brief Pointer to the table of object functions.
	struct allio_abi_object_functions const* functions;
}
typedef allio_abi_object;

/// @brief Table of functions available to the user of an opaque object.
struct allio_abi_object_functions
{
	/// @brief Close the object.
	void(*close)(allio_abi_object* object);

	/// @brief Notify the object of poll completion. The user of an opaque object should invoke this
	///        function when polling the object completes successfully.
	/// @param information Platform specific poll result information.
	///                    * Posix:   Poll event mask.
	///                    * Windows: Not used.
	allio_abi_result(*notify)(allio_abi_object* object, uintptr_t information);
}
typedef allio_abi_object_functions;

#ifdef __cplusplus
} // extern "C"
#endif
