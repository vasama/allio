#pragma once

#include <allio/path_view.hpp>

#include <span>

#include <cstring>

namespace allio {

template<typename Char>
void generate_unique_name(std::span<Char> buffer);

extern template void generate_unique_name<char>(std::span<char> buffer);
extern template void generate_unique_name<wchar_t>(std::span<wchar_t> buffer);


template<typename Char, size_t Size, size_t ExtensionSize>
class unique_name
{
	static_assert(Size > 0);
	static_assert(ExtensionSize > 0);

	static constexpr size_t size = Size + ExtensionSize;
	Char m_buffer[size];

public:
	unique_name(std::integral_constant<size_t, Size>, Char const(&extension)[ExtensionSize + 1])
	{
		memcpy(m_buffer + Size, extension, ExtensionSize * sizeof(Char));
	}

	basic_path_view<Char> generate()
	{
		generate_unique_name(std::span<Char>(m_buffer, Size));
		return basic_path_view<Char>(std::basic_string_view<Char>(m_buffer, size));
	}
};

template<typename Char, size_t Size, size_t ExtensionSize>
unique_name(std::integral_constant<size_t, Size>, Char const(&)[ExtensionSize]) -> unique_name<Char, Size, ExtensionSize - 1>;

} // namespace allio
