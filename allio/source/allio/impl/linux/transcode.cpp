#include <allio/impl/transcode.hpp>

#include <vsm/concepts.hpp>

using namespace allio;
using namespace allio::detail;

template<typename Char>
concept utf8_character = character<Char> && vsm::any_of<Char, char, char8_t>;

static transcode_result _transcode(
	char const* const decode_data,
	size_t const decode_size,
	char* const encode_data,
	size_t const encode_size)
{
	size_t const size = std::min(decode_size, encode_size);

	transcode_result r =
	{
		.decoded = size,
		.encoded = size,
	};

	if (encode_data != nullptr)
	{
		memmove(encode_data, decode_data, size);
	}

	if (decode_size > encode_size)
	{
		r.ec = transcode_error::no_buffer_space;
	}

	return r;
}

template<utf8_character SourceChar, utf8_character TargetChar>
static transcode_result _transcode(
	SourceChar const* const decode_data,
	size_t const decode_size,
	TargetChar* const encode_data,
	size_t const encode_size)
{
	return _transcode(
		reinterpret_cast<char const*>(decode_data),
		decode_size,
		reinterpret_cast<char*>(encode_data),
		encode_size);
}

template<character SourceChar, character TargetChar>
static transcode_result _transcode(
	SourceChar const* const decode_data,
	size_t const decode_size,
	TargetChar* const encode_data,
	size_t const encode_size)
{
	return { transcode_error::unsupported_operation };
}

template<character TargetChar, character SourceChar>
transcode_result allio::transcode_size(
	std::basic_string_view<SourceChar> const decode_buffer,
	size_t const max_encoded_size)
{
	return _transcode(
		decode_buffer.data(),
		decode_buffer.size(),
		static_cast<TargetChar*>(nullptr),
		max_encoded_size);
}

template<character TargetChar, character SourceChar>
transcode_result allio::transcode(
	std::basic_string_view<SourceChar> const decode_buffer,
	std::span<TargetChar> const encode_buffer)
{
	return _transcode(
		decode_buffer.data(),
		decode_buffer.size(),
		encode_buffer.data(),
		encode_buffer.size());
}

#define allio_detail_transcode_instance(S, T) \
	template transcode_result allio::transcode_size<T, S>(std::basic_string_view<S>, size_t); \
	template transcode_result allio::transcode<T, S>(std::basic_string_view<S>, std::span<T>); \

allio_detail_transcode_instance(char, char);
allio_detail_transcode_instance(char, wchar_t);
allio_detail_transcode_instance(char, char8_t);
allio_detail_transcode_instance(char, char16_t);
allio_detail_transcode_instance(char, char32_t);

allio_detail_transcode_instance(wchar_t, char);
allio_detail_transcode_instance(wchar_t, wchar_t);
allio_detail_transcode_instance(wchar_t, char8_t);
allio_detail_transcode_instance(wchar_t, char16_t);
allio_detail_transcode_instance(wchar_t, char32_t);

allio_detail_transcode_instance(char8_t, char);
allio_detail_transcode_instance(char8_t, wchar_t);
allio_detail_transcode_instance(char8_t, char8_t);
allio_detail_transcode_instance(char8_t, char16_t);
allio_detail_transcode_instance(char8_t, char32_t);

allio_detail_transcode_instance(char16_t, char);
allio_detail_transcode_instance(char16_t, wchar_t);
allio_detail_transcode_instance(char16_t, char8_t);
allio_detail_transcode_instance(char16_t, char16_t);
allio_detail_transcode_instance(char16_t, char32_t);

allio_detail_transcode_instance(char32_t, char);
allio_detail_transcode_instance(char32_t, wchar_t);
allio_detail_transcode_instance(char32_t, char8_t);
allio_detail_transcode_instance(char32_t, char16_t);
allio_detail_transcode_instance(char32_t, char32_t);
