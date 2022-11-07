#include <allio/file_handle.hpp>
#include <allio/linux/io_uring_multiplexer.hpp>

#include <allio/implementation.hpp>

#include <allio/impl/linux/io_uring_byte_io.hpp>
#include <allio/impl/linux/io_uring_filesystem_handle.hpp>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::linux;

allio_HANDLE_MULTIPLEXER_IMPLEMENTATION(io_uring_multiplexer, file_handle);
