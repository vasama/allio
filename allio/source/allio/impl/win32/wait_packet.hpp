#pragma once

#include <allio/impl/win32/kernel.hpp>
#include <allio/win32/detail/wait_packet.hpp>

#include <vsm/result.hpp>

namespace allio::win32 {

/// @brief Create a wait packet object.
vsm::result<detail::unique_handle> create_wait_packet();

/// @brief Submit a wait for a handle using a wait packet.
///        The waiting happens as if by WaitForSingleObject, but when completed,
///        posts a completion to the specified completion port's completion queue.
/// @param wait_packet The wait packet object used to perform the wait.
/// @param completion_port The completion port object to which a a completion
///        is posted upon completion of the wait.
/// @param target_handle The handle being awaited.
/// @param key_context The KeyContext value posted to the completion port queue.
/// @param apc_context The ApcContext value posted to the completion port queue.
/// @param completion_status The Status value posted to the completion port queue.
/// @param completion_information The Information value posted to the completion port queue.
/// @return True if the target handle was already signaled.
/// @note A wait packet object can only be used to wait on a single handle at a time.
///       To perform multiple waits, multiple wait packet objects must be used.
/// @note A completion is posted to the completion port queue regardless of whether
///       the target handle was already signaled.
vsm::result<bool> associate_wait_packet(
	HANDLE wait_packet,
	HANDLE completion_port,
	HANDLE target_handle,
	PVOID key_context,
	PVOID apc_context,
	NTSTATUS completion_status,
	ULONG_PTR completion_information);

/// @brief Attempt to cancel a wait submitted on a wait packet.
/// @returns True if the wait was cancelled.
vsm::result<bool> cancel_wait_packet(
	HANDLE wait_packet,
	bool remove_queued_completion);

} // namespace allio::win32
