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
consteval uint32_t encode_file_name(
	std::string_view const root_path,
	std::string_view file_name)
{
#if vsm_os_win32
	constexpr auto compare = [](
		std::string_view const lhs,
		std::string_view const rhs) -> std::strong_ordering
	{
		return std::lexicographical_compare_three_way(
			lhs.begin(),
			lhs.end(),
			rhs.begin(),
			rhs.end(),
			[](char const lhs, char const rhs) -> std::strong_ordering
			{
				if ((lhs == '\\' || lhs == '/') && (rhs == '\\' || rhs == '/'))
				{
					return std::strong_ordering::equivalent;
				}

				return lhs <=> rhs;
			});
	};
#else
	constexpr auto compare = std::compare_three_way();
#endif

	if (compare(root_path, file_name.substr(0, root_path.size())) == 0)
	{
		file_name.remove_prefix(root_path.size());
	}

	constexpr char const* const* const beg = std::begin(Encoding::file_names);
	constexpr char const* const* const end = std::end(Encoding::file_names);
	static_assert(end - beg < 1 << file_bits);

	auto const pos = std::lower_bound(
		beg + 1, // Skip over the unknown file entry.
		end,
		file_name,
		[&](std::string_view const lhs, std::string_view const rhs) -> bool
		{
			return compare(lhs, rhs) < 0;
		});

	if (pos != end && compare(*pos, file_name) == 0)
	{
		return static_cast<uint32_t>(pos - beg);
	}

	return 0;
}

template<typename Encoding>
consteval uint32_t encode_location(
	std::string_view const root_path,
	std::source_location const& location)
{
	if (location.line() < 1 << line_bits)
	{
		constexpr uint32_t line_range = static_cast<uint32_t>(1) << line_bits;
		uint32_t const file = encode_file_name<Encoding>(root_path, location.file_name());
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


#ifdef __INTELLISENSE__
#	define allio_error(...) (__VA_ARGS__)
#else
#	define allio_error(...) ( \
		::allio::detail::ec::encode< \
			allio_error_encoding, \
			::allio::detail::ec::encode_location<allio_error_encoding>( \
				allio_error_encoding_path "/", \
				std::source_location::current()) \
		>(__VA_ARGS__) \
	)
#endif

} // namespace allio::detail::ec
