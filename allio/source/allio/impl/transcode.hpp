#pragma once

#include <allio/any_string.hpp>
#include <allio/any_string_buffer.hpp>
#include <allio/encoding.hpp>

#include <vsm/assert.h>
#include <vsm/lift.hpp>

#include <span>
#include <string_view>

namespace allio {

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
	std::basic_string_view<SourceChar> const decode_buffer,
	std::span<TargetChar> const encode_buffer)
{
	transcode_result const r = transcode(decode_buffer, encode_buffer);
	vsm_assert(r.ec == transcode_error{});
	return r;
}


namespace detail {

template<detail::character TargetChar, detail::character SourceChar>
vsm::result<size_t> _transcode_string(
	std::basic_string_view<SourceChar> const decode_buffer,
	string_buffer<TargetChar> const encode_buffer)
{
	vsm_try(out_buffer_1, encode_buffer.resize(0, static_cast<size_t>(-1)));
	auto const r1 = transcode(decode_buffer, out_buffer_1);
	size_t encoded_size = r1.encoded;

	if (r1.ec == transcode_error{})
	{
		if (encoded_size != out_buffer_1.size())
		{
			vsm_try_discard(encode_buffer.resize(r1.encoded));
		}
	}
	else
	{
		if (r1.ec != transcode_error::no_buffer_space)
		{
			return vsm::unexpected(error::invalid_encoding);
		}

		auto const r2 = transcode_size<TargetChar>(
			decode_buffer.substr(r1.decoded));

		if (r2.ec != transcode_error{})
		{
			return r2.ec == transcode_error::no_buffer_space
				? vsm::unexpected(error::no_buffer_space)
				: vsm::unexpected(error::invalid_encoding);
		}

		vsm_try(out_buffer_2, encode_buffer.resize(r1.encoded + r2.encoded));

		// Resizing the encode buffer again does not overwrite the content already written into it.
		vsm_assert(memcmp(out_buffer_1.data(), out_buffer_2.data(), r1.encoded) == 0);

		transcode_unchecked(
			decode_buffer.substr(r1.decoded),
			out_buffer_2.subspan(r1.encoded));

		encoded_size += r2.encoded;
	}

	return {};
}

} // namespace detail

template<detail::character SourceChar>
vsm::result<size_t> transcode_string(
	std::basic_string_view<SourceChar> const decode_buffer,
	any_string_buffer const encode_buffer)
{
	return encode_buffer.visit(vsm_bind_borrow(detail::_transcode_string, decode_buffer));
}

} // namespace allio
