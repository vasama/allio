#pragma once

#include <allio_error_encoding.hpp>

#include <source_location>
#include <system_error>

#include <cstdint>
#include <cstring>

namespace allio::detail::ec {

inline constexpr int line_bits                  = 12;
inline constexpr int file_bits                  = 8;
inline constexpr int code_bits                  = 12;

template<typename Encoding, typename ErrorCode>
class encoded_error_category : public std::error_category
{
public:
	char const* name() const noexcept override;
	std::string message(int code) const override;
	std::error_condition default_error_condition(int code) const noexcept override;

	static encoded_error_category const instance;
};

template<typename ErrorCode>
uint32_t encode_error_code(ErrorCode e);

template<typename ErrorCode>
ErrorCode decode_error_code(uint32_t e);

template<typename Encoding>
consteval uint32_t encode_file_name(char const* const file_name)
{
	constexpr char const* const* const beg = std::begin(Encoding::file_names);
	constexpr char const* const* const end = std::end(Encoding::file_names);
	static_assert(end - beg < 1 << file_bits);

	auto const pos = std::lower_bound(
		beg,
		end,
		file_name,
		[](char const* const lhs, char const* const rhs)
		{
			return std::strcmp(lhs, rhs) < 0;
		});

	return static_cast<uint32_t>((pos + 1) - Encoding::file_names);
}

template<typename Encoding>
consteval uint32_t encode_location(std::source_location const& location)
{
	if (location.line() < 1 << line_bits)
	{
		constexpr uint32_t line_range = static_cast<uint32_t>(1) << line_bits;
		uint32_t const file = encode_file_name<Encoding>(location.file_name());
		uint32_t const line = location.line() < line_range ? location.line() : 0;
		return (line << file_bits | file) << code_bits;
	}
}

template<typename Encoding, uint32_t Location, typename ErrorCode>
	requires std::is_error_code_enum_v<ErrorCode>
[[nodiscard]] std::error_code encode(ErrorCode const error_code)
{
	if (uint32_t const code = encode_error_code(error_code))
	{
		return std::error_code(
			static_cast<int>(Location | code),
			encoded_error_category<Encoding, ErrorCode>::instance);
	}

	return error_code;
}

#define allio_error(...) ( \
		::allio::detail::ec::encode< \
			allio_error_encoding, \
			::allio::detail::ec::encode_location<allio_error_encoding>( \
				std::source_location::current()) \
		>(__VA_ARGS__) \
	)

} // namespace allio::detail::ec
