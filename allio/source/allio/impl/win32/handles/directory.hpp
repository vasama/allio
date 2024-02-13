#pragma once

#include <allio/detail/handles/directory.hpp>

#include <allio/impl/win32/handles/fs_object.hpp>
#include <allio/impl/win32/kernel.hpp>

namespace allio::win32 {

NTSTATUS query_directory_file_start(
	HANDLE handle,
	HANDLE event,
	read_buffer buffer,
	bool restart,
	PIO_APC_ROUTINE apc_routine,
	PVOID apc_context,
	IO_STATUS_BLOCK& io_status_block);

NTSTATUS query_directory_file_completed(
	read_buffer buffer,
	NTSTATUS status,
	ULONG_PTR information,
	detail::directory_stream_native_handle& out_stream);

} // namespace allio::win32
