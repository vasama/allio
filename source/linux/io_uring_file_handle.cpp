#include <allio/file_handle.hpp>
#include <allio/linux/io_uring_multiplexer.hpp>

#include <allio/static_multiplexer_handle_relation_provider.hpp>

#include "io_uring_byte_io.hpp"
#include "io_uring_filesystem_handle.hpp"

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::linux;

allio_MULTIPLEXER_HANDLE_RELATION(io_uring_multiplexer, allio::file_handle);
