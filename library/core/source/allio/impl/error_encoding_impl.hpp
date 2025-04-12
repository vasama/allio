#pragma once

#include <allio/impl/error_encoding.hpp>

#include <charconv>
#include <format>

namespace allio::detail::ec {

inline constexpr uint32_t line_mask = (static_cast<uint32_t>(1) << line_bits) - 1;
inline constexpr uint32_t file_mask = (static_cast<uint32_t>(1) << file_bits) - 1;
inline constexpr uint32_t code_mask = (static_cast<uint32_t>(1) << code_bits) - 1;

template<typename ErrorCode>
struct decoded_error
{
	char const* file;
	int line;
	ErrorCode code;
};

template<typename Encoding, typename ErrorCode>
decoded_error<ErrorCode> _decode_error_code(int const e)
{
	uint32_t encoded = static_cast<uint32_t>(e);

	uint32_t const code = encoded & code_mask;
	encoded >>= code_bits;
	uint32_t const file = encoded & file_mask;
	encoded >>= file_bits;
	uint32_t const line = encoded & line_mask;

	return
	{
		.file = Encoding::file_names[file],
		.line = static_cast<int>(line),
		.code = decode_error_code<ErrorCode>(code),
	};
}

template<typename Encoding, typename ErrorCode>
char const* encoded_error_category<Encoding, ErrorCode>::name() const noexcept
{
	return make_error_code(static_cast<ErrorCode>(0)).category().name();
}

template<typename Encoding, typename ErrorCode>
std::string encoded_error_category<Encoding, ErrorCode>::message(int const code) const
{
	auto const decoded = _decode_error_code<Encoding, ErrorCode>(code);
	auto const message = make_error_code(decoded.code).message();

	std::string_view const space = message.ends_with('\n')
		? ""
		: " ";

	return std::format("{}{}[{}:{}]", message, space, decoded.file, decoded.line);
}

template<typename Encoding, typename ErrorCode>
std::error_condition encoded_error_category<Encoding, ErrorCode>::default_error_condition(
	int const code) const noexcept
{
	auto const decoded = _decode_error_code<Encoding, ErrorCode>(code);
	return make_error_code(decoded.code).default_error_condition();
}

template<typename Encoding, typename ErrorCode>
std::error_code encoded_error_category<Encoding, ErrorCode>::unwrap(
	std::error_code const ec) const noexcept
{
	vsm_assert(ec.category() == *this); //PRECONDITION
	auto const decoded = _decode_error_code<Encoding, ErrorCode>(ec.value());
	return std::error_code(static_cast<int>(decoded.code), *this);
}

template<typename Encoding, typename ErrorCode>
inline encoded_error_category<Encoding, ErrorCode>
const encoded_error_category<Encoding, ErrorCode>::instance;

} // namespace allio::detail::ec
