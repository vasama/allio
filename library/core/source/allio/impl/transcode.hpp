#pragma once

#include <allio/any_string.hpp>
#include <allio/any_string_buffer.hpp>
#include <allio/detail/transcode.hpp>

#include <vsm/assert.h>

#include <cstring>

namespace allio {

using detail::transcode_error;
using detail::transcode_result;

using detail::transcode_size;
using detail::transcode;
using detail::transcode_unchecked;

template<detail::character TargetChar, detail::character SourceChar>
vsm::result<size_t> transcode_string(
	std::basic_string_view<SourceChar> const decode_buffer,
	string_buffer<TargetChar> const encode_buffer)
{
	vsm_try(out_buffer_1, encode_buffer.resize(
		//TODO: Should this pass 1 instead?
		0,
		static_cast<size_t>(-1)));

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

		#if vsm_config_assert > 0
		using target_string_view = std::basic_string_view<TargetChar>;

		auto const hash_string = [](auto const& string)
		{
			return std::hash<target_string_view>()(target_string_view(string));
		};

		size_t const hash_1 = hash_string(out_buffer_1.subspan(0, r1.encoded));
		#endif

		vsm_try(out_buffer_2, encode_buffer.resize(r1.encoded + r2.encoded));

		// Resizing the encode buffer again does not overwrite the content already written into it.
		vsm_assert(hash_1 == hash_string(out_buffer_2.subspan(0, r1.encoded)));

		transcode_unchecked(
			decode_buffer.substr(r1.decoded),
			out_buffer_2.subspan(r1.encoded));

		encoded_size += r2.encoded;
	}

	return encoded_size;
}

template<detail::character SourceChar>
vsm::result<size_t> transcode_string(
	std::basic_string_view<SourceChar> const decode_buffer,
	any_string_buffer const encode_buffer)
{
	return encode_buffer.visit([&](auto const encode_buffer)
	{
		return transcode_string(decode_buffer, encode_buffer);
	});
}

template<detail::character TargetChar>
vsm::result<size_t> transcode_string(
	any_string_view const decode_buffer,
	string_buffer<TargetChar> const encode_buffer)
{
	return decode_buffer.visit([&](auto const decode_buffer) -> vsm::result<size_t>
	{
		using decode_buffer_type = std::remove_cv_t<decltype(decode_buffer)>;

		if constexpr (std::is_same_v<decode_buffer_type, detail::string_length_out_of_range_t>)
		{
			return vsm::unexpected(error::argument_too_long);
		}
		else
		{
			return transcode_string(decode_buffer, encode_buffer);
		}
	});
}

} // namespace allio
