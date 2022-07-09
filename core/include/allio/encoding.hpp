#pragma once

namespace allio {

enum class encoding
{
	narrow_execution_encoding = 1,
	wide_execution_encoding,

	utf8,
	utf16,
	utf32,
};

namespace detail {

template<typename Char>
struct _encoding_of
{
};

template<>
struct _encoding_of<char>
{
	static constexpr encoding value = encoding::narrow_execution_encoding;
};

template<>
struct _encoding_of<wchar_t>
{
	static constexpr encoding value = encoding::wide_execution_encoding;
};

template<>
struct _encoding_of<char8_t>
{
	static constexpr encoding value = encoding::utf8;
};

template<>
struct _encoding_of<char16_t>
{
	static constexpr encoding value = encoding::utf16;
};

template<>
struct _encoding_of<char32_t>
{
	static constexpr encoding value = encoding::utf32;
};

template<typename Char>
concept character = requires { _encoding_of<Char>::value; };

template<character Char>
inline constexpr encoding encoding_of = _encoding_of<Char>::value;

} // namespace detail
} // namespace allio
