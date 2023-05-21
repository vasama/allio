#pragma once

#include <allio/directory_handle.hpp>

#include <allio/impl/win32/filesystem_handle.hpp>
#include <allio/impl/win32/kernel.hpp>

namespace allio {
namespace win32 {

NTSTATUS query_directory_file_start(
	io::parameters_with_result<io::directory_read> const& args,
	PIO_APC_ROUTINE apc_routine, PVOID apc_context,
	IO_STATUS_BLOCK& io_status_block);

NTSTATUS query_directory_file_completed(
	io::parameters_with_result<io::directory_read> const& args,
	IO_STATUS_BLOCK const& io_status_block);

#if 0
using detail::directory_stream_native_handle;
using detail::directory_stream_iterator_native_handle;

struct directory_stream
{
	// Restart the stream on the next fetch.
	bool restart = true;

	// Some data has already fetched into the buffer.
	bool any_ready = false;

	// More data can be fetched from the stream.
	bool can_fetch = false;

	// Offset of the first entry in the buffer.
	uint16_t entry_offset;

	std::byte buffer alignas(uintptr_t)[];
};

inline directory_stream_native_handle wrap_directory_stream(directory_stream* const stream)
{
	return std::bit_cast<directory_stream_native_handle>(stream);
}

inline directory_stream* unwrap_directory_stream(directory_stream_native_handle const handle)
{
	return std::bit_cast<directory_stream*>(handle);
}


vsm::result<directory_stream*> get_or_create_directory_stream(directory_stream_native_handle& handle);

NTSTATUS query_directory_file_start(
	directory_stream& stream, HANDLE handle,
	PIO_APC_ROUTINE apc_routine, PVOID apc_context,
	IO_STATUS_BLOCK& io_status_block);

NTSTATUS query_directory_file_completed(
	directory_stream& stream,
	IO_STATUS_BLOCK const& io_status_block);
#endif

} // namespace win32
} // namespace allio
