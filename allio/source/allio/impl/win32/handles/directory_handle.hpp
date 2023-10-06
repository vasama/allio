#pragma once

#include <allio/directory_handle.hpp>

#include <allio/impl/win32/filesystem_handle.hpp>
#include <allio/impl/win32/kernel.hpp>

namespace allio::win32 {

NTSTATUS query_directory_file_start(
	io::parameters_with_result<io::directory_read> const& args,
	PIO_APC_ROUTINE apc_routine, PVOID apc_context,
	IO_STATUS_BLOCK& io_status_block);

NTSTATUS query_directory_file_completed(
	io::parameters_with_result<io::directory_read> const& args,
	IO_STATUS_BLOCK const& io_status_block);

} // namespace allio::win32
