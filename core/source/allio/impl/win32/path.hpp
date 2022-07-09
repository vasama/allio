#pragma once

#include <allio/impl/win32/peb.hpp>

#include <allio/impl/win32/api_string.hpp>

namespace allio {
namespace win32 {

vsm::result<wchar_t const*> make_winapi_path(
	api_string_storage& storage,
	std::wstring_view root_path,
	any_string_view path);

} // namespace win32
} // namespace allio
