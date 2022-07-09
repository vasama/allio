#include <allio/file_handle.hpp>
#include <allio/win32/iocp_multiplexer.hpp>

#include <allio/win32/kernel_error.hpp>
#include <allio/win32/platform.hpp>
#include <allio/static_multiplexer_handle_relation_provider.hpp>

#include "iocp_byte_io.hpp"
#include "iocp_filesystem_handle.hpp"

using namespace allio;
using namespace allio::win32;

allio_MULTIPLEXER_HANDLE_RELATION(iocp_multiplexer, file_handle);
