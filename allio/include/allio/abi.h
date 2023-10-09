#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum
{
	allio_abi_version = 1,
};

enum allio_abi_result
{
	/// @brief The operation completed successfully.
	allio_abi_result_success = 0,

	/// @brief The operation could not be completed.
	///        Retry by polling the handle again.
	allio_abi_result_try_again,
}
typedef allio_abi_result;

/// @brief ABI stable native handle type for asynchronous polling across ABI boundaries such as
///        dynamically linked library interfaces. Instead of exposing allio C++ types in your
///        library interface, return an a pointer to opaque_handle and wrap it in a
///        @ref opaque_handle before returning it to the user of your library.
struct allio_abi_handle
{
	uint32_t version;

	/// @brief Platform specific pollable handle value.
	///        * Posix: file descriptor
	///        * Windows: HANDLE
	uintptr_t handle_value;

	/// @brief Platform specific information describing the handle and how to poll it.
	///        * Posix: Bitmask of POLL* flags.
	///        * Windows: not used.
	uintptr_t handle_information;

	/// @brief Pointer to the table of handle functions.
	struct allio_abi_handle_functions const* functions;
}
typedef allio_abi_handle;

/// @brief Table of functions available to the user of an opaque handle.
struct allio_abi_handle_functions
{
	/// @brief Close the handle.
	void(*close)(allio_abi_handle* handle);

	/// @brief Notify the handle of poll completion.
	///        The user of an opaque handle should invoke this function
	///        when polling the handle completes successfully.
	/// @param information Platform specific poll result information.
	///                    * Posix: Poll event mask.
	///                    * Windows: Not used.
	allio_abi_result(*notify)(allio_abi_handle* handle, uintptr_t information);
}
typedef allio_abi_handle_functions;

#ifdef __cplusplus
} // extern "C"
#endif
