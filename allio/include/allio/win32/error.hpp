#pragma once

#include <allio/error.hpp>

namespace allio {

enum class error_source : uintptr_t
{
	NtClose,
	close,
	iocp_multiplexer_detach,
};

} // namespace allio
