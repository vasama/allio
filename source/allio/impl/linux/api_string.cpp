#include "api_string.hpp"

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::linux;

result<void> api_string::reserve(size_t const size)
{
	if (size > 0)
	{
		allio_TRYV(m_buffer.reserve(size + 1));
		m_size = size;
	}
	return {};
}

result<char*> api_string::set(std::string_view const string)
{
	allio_TRYV(reserve(string.size()));
	if (!string.empty())
	{
		char* const data = m_buffer.data();
		memcpy(data, string.data(), string.size());
		data[string.size()] = '\0';
	}
	return c_string();
}

result<char const* const*> api_strings::set(std::span<std::string_view const> const strings)
{
	size_t const total_size = std::accumulate(
		strings.begin(), strings.end(), static_cast<size_t>(0),
		[](size_t const a, std::string_view const s) { return a + s.size() + 1; });

	allio_TRYV(m_buffer.reserve(total_size + (strings.size() + 1) * sizeof(char const*)));

	char const** c_strings = reinterpret_cast<char const**>(m_buffer.data());
	char* c_string_data = reinterpret_cast<char*>(c_strings + strings.size() + 1);

	for (std::string_view const string : strings)
	{
		*c_strings++ = c_string_data;
		c_string_data = std::copy(string.begin(), string.end(), c_string_data);
		*c_string_data++ = '\0';
	}
	*c_strings = nullptr;

	return c_strings();
}
