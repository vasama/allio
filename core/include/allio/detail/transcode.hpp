#pragma once

#include <allio/encoding.hpp>

#include <span>
#include <string_view>

namespace allio::detail {

enum class transcode_error
{
	no_buffer_space = 1,
	invalid_source_encoding,
	unsupported_operation,
};

struct transcode_result
{
	transcode_error ec;
	size_t decoded;
	size_t encoded;
};

template<detail::character TargetChar, detail::character SourceChar>
[[nodiscard]] transcode_result transcode_size(
	std::basic_string_view<SourceChar> decode_buffer,
	size_t max_encoded_size = static_cast<size_t>(-1));

template<detail::character TargetChar, detail::character SourceChar>
[[nodiscard]] transcode_result transcode(
	std::basic_string_view<SourceChar> decode_buffer,
	std::span<TargetChar> encode_buffer);

template<detail::character TargetChar, detail::character SourceChar>
transcode_result transcode_unchecked(
	std::basic_string_view<SourceChar> decode_buffer,
	std::span<TargetChar> encode_buffer);

} // namespace allio::detail
