#pragma once

#include <allio/encoding.hpp>

#include <vsm/concepts.hpp>

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

template<vsm::utf_character TargetChar, vsm::utf_character SourceChar>
[[nodiscard]] transcode_result transcode_size(
	std::basic_string_view<SourceChar> decode_buffer,
	size_t max_encoded_size = static_cast<size_t>(-1));

#ifndef allio_detail_transcode_gcc_workaround
template<vsm::character TargetChar, vsm::character SourceChar>
[[nodiscard]] transcode_result transcode_size(
	std::basic_string_view<SourceChar> const decode_buffer,
	size_t const max_encoded_size = static_cast<size_t>(-1))
{
	return detail::transcode_size<detail::as_utf_char_t<TargetChar>>(
		detail::reinterpret_as_utf(decode_buffer),
		max_encoded_size);
}
#endif

template<vsm::utf_character TargetChar, vsm::utf_character SourceChar>
[[nodiscard]] transcode_result transcode(
	std::basic_string_view<SourceChar> decode_buffer,
	std::span<TargetChar> encode_buffer);

#ifndef allio_detail_transcode_gcc_workaround
template<vsm::character TargetChar, vsm::character SourceChar>
[[nodiscard]] transcode_result transcode(
	std::basic_string_view<SourceChar> const decode_buffer,
	std::span<TargetChar> const encode_buffer)
{
	return detail::transcode(
		detail::reinterpret_as_utf(decode_buffer),
		detail::reinterpret_as_utf(encode_buffer));
}
#endif

template<vsm::utf_character TargetChar, vsm::utf_character SourceChar>
transcode_result transcode_unchecked(
	std::basic_string_view<SourceChar> decode_buffer,
	std::span<TargetChar> encode_buffer);

#ifndef allio_detail_transcode_gcc_workaround
template<vsm::character TargetChar, vsm::character SourceChar>
transcode_result transcode_unchecked(
	std::basic_string_view<SourceChar> const decode_buffer,
	std::span<TargetChar> const encode_buffer)
{
	return detail::transcode_unchecked(
		detail::reinterpret_as_utf(decode_buffer),
		detail::reinterpret_as_utf(encode_buffer));
}
#endif

} // namespace allio::detail
