#pragma once

#include <allio/deadline.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/win32/detail/unique_handle.hpp>

#include <vsm/result.hpp>

#include <span>

namespace allio::win32 {

vsm::result<detail::unique_handle> create_completion_port(
	size_t max_concurrent_threads);

vsm::result<void> set_completion_information(
	HANDLE handle,
	HANDLE completion_port,
	void* key_context);

vsm::result<void> set_io_completion(
	HANDLE completion_port,
	void* key_context,
	void* apc_context,
	NTSTATUS completion_status,
	ULONG completion_information);

vsm::result<size_t> remove_io_completions(
	HANDLE completion_port,
	std::span<FILE_IO_COMPLETION_INFORMATION> buffer,
	deadline deadline);

} // namespace allio::win32
