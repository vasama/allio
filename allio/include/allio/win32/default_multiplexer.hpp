#pragma once

#include <allio/win32/iocp_multiplexer.hpp>

#define allio_detail_default_multiplexer iocp

namespace allio {

using default_multiplexer = win32::iocp_multiplexer;

} // namespace allio
