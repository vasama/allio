#define WIN32_NLS

#include "api_string.hpp"

#include <allio/detail/assert.hpp>

#include <win32.h>

using namespace allio;
using namespace allio::win32;

void api_string::append(std::string_view const string)
{
	size_t const size = MultiByteToWideChar(CP_UTF8, 0,
		string.data(), string.size(), nullptr, 0);

	size_t const offset = basic_string::size();

	basic_string::resize(offset + size);
	wchar_t* const data = basic_string::data();

	allio_VERIFY(size == MultiByteToWideChar(CP_UTF8, 0,
		string.data(), string.size(), data + offset, size));
}
