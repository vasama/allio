#pragma once

#include <allio/impl/win32/api_string.hpp>

namespace allio::win32 {

vsm::result<wchar_t*> make_command_line(
	api_string_storage& storage,
	any_string_view program,
	any_string_span arguments);

} // namespace allio::win32
