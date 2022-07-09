#pragma once

#include <vsm/assert.h>

#include <span>

#include <cstddef>

namespace allio::detail {

static constexpr size_t hexadecimal_size(size_t const bytes_size)
{
	size_t const bits_per_char = 4;
	size_t const chars_per_byte = CHAR_BIT / bits_per_char;
	return bytes_size * chars_per_byte;
}

template<typename Char>
static constexpr size_t make_hexadecimal(std::span<std::byte const> const bytes, std::span<Char> const buffer)
{
	vsm_assert(buffer.size() >= hexadecimal_size(bytes.size())); //PRECONDITION

	constexpr size_t bits_per_char = 4;
	constexpr size_t chars_per_byte = CHAR_BIT / bits_per_char;

	Char* out = buffer.data();
	for (std::byte const byte : bytes)
	{
		auto value = static_cast<unsigned char>(byte);
		for (size_t i = 0; i < chars_per_byte; ++i)
		{
			auto const v = value & 0x0f;
			*out++ = static_cast<Char>((v < 10 ? '0' : 'a') + v);
			value >>= bits_per_char;
		}
	}
	return static_cast<size_t>(out - buffer.data());
}

} // namespace allio::detail
