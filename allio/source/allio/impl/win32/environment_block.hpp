#pragma once

#include <allio/impl/win32/api_string.hpp>

namespace allio::win32 {

vsm::result<wchar_t*> make_environment_block(
	api_string_storage& storage,
	any_string_span environment);

} // namespace allio::win32
