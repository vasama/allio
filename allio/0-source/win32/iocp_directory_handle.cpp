#include <allio/directory_handle.hpp>
#include <allio/win32/iocp_multiplexer.hpp>

#include <allio/implementation.hpp>

#include <allio/win32/nt_error.hpp>
#include <allio/win32/platform.hpp>

#include <allio/impl/win32/iocp_filesystem_handle.hpp>
#include <allio/impl/win32/sync_filesystem_handle.hpp>

using namespace allio;
using namespace allio::win32;

allio_handle_multiplexer_implementation(iocp_multiplexer, directory_handle);
