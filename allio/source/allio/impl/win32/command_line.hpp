#pragma once

#include <allio/any_string.hpp>

#include <vsm/result.hpp>

#include <memory>

namespace allio::win32 {

class command_line_builder;

class command_line_storage
{
	std::unique_ptr<wchar_t[]> m_dynamic;
	wchar_t m_static[(1024 - sizeof(m_dynamic)) / sizeof(wchar_t)];

public:
	command_line_storage() = default;

	command_line_storage(command_line_storage const&) = delete;
	command_line_storage& operator=(command_line_storage const&) = delete;

private:
	friend command_line_builder;
};


vsm::result<wchar_t*> make_command_line(
	command_line_storage& storage,
	any_string_view program,
	any_string_span arguments);

} // namespace allio::win32
