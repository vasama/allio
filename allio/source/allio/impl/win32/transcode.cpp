#define WIN32_NLS

#include <allio/impl/transcode.hpp>

#include <allio/impl/win32/error.hpp>

#include <vsm/assert.h>
#include <vsm/integer_conversion.hpp>

#include <win32.h>

using namespace allio;
using namespace allio::detail;

template<typename Char>
concept native_character = character<Char> && vsm::any_of<Char, char, wchar_t>;

template<character Char>
struct _native_character;

template<> struct _native_character<char> { using type = char; };
template<> struct _native_character<wchar_t> { using type = wchar_t; };
template<> struct _native_character<char8_t> { using type = char; };
template<> struct _native_character<char16_t> { using type = wchar_t; };

template<character Char>
using native_character_t = typename _native_character<Char>::type;


template<native_character SourceChar, native_character TargetChar>
static int transcode_win32_enter(
	UINT const code_page,
	SourceChar const* const source_data,
	int const source_size,
	TargetChar* const target_data,
	int const target_size)
{
	/**/ if constexpr (sizeof(SourceChar) == 1 && sizeof(TargetChar) == 2)
	{
		return MultiByteToWideChar(
			code_page,
			MB_ERR_INVALID_CHARS,
			reinterpret_cast<char const*>(source_data),
			source_size,
			reinterpret_cast<wchar_t*>(target_data),
			target_size);
	}
	else if constexpr (sizeof(SourceChar) == 2 && sizeof(TargetChar) == 1)
	{
		return WideCharToMultiByte(
			code_page,
			WC_ERR_INVALID_CHARS,
			reinterpret_cast<wchar_t const*>(source_data),
			source_size,
			reinterpret_cast<char*>(target_data),
			target_size,
			/* lpDefaultChar: */ nullptr,
			/* lpUsedDefaultChar: */ nullptr);
	}
	else
	{
		static_assert(sizeof(SourceChar) == 0);
	}
}

template<native_character SourceChar, native_character TargetChar>
static transcode_result transcode_win32_check(
	UINT const code_page,
	SourceChar const* const source_data,
	int const source_size,
	TargetChar* const target_data,
	int const target_size)
{
	vsm_assert(source_size > 0);

	transcode_result r = {};

	int const size = transcode_win32_enter(
		code_page,
		source_data,
		source_size,
		target_data,
		target_size);

	if (size == 0)
	{
		switch (GetLastError())
		{
		case ERROR_INSUFFICIENT_BUFFER:
			r.ec = transcode_error::no_buffer_space;
			break;

		case ERROR_NO_UNICODE_TRANSLATION:
			r.ec = transcode_error::invalid_source_encoding;
			break;

		default:
			vsm_assert(false);
		}
	}
	else
	{
		r.decoded = static_cast<size_t>(source_size);
		r.encoded = static_cast<size_t>(size);
	}

	return r;
}

template<native_character SourceChar, native_character TargetChar>
static transcode_result transcode_win32(
	UINT const code_page,
	SourceChar const* const decode_data,
	size_t const decode_size,
	TargetChar* const encode_data,
	size_t const encode_size)
{
	transcode_result r = {};
	while (r.decoded < decode_size)
	{
		if (r.encoded >= encode_size)
		{
			r.ec = transcode_error::no_buffer_space;
			break;
		}

		TargetChar* target_data = nullptr;
		int target_size = 0;

		if (encode_data != nullptr)
		{
			target_data = encode_data + r.encoded;
			target_size = vsm::saturating(encode_size - r.encoded);
		}

		auto const r2 = transcode_win32_check(
			code_page,
			decode_data + r.decoded,
			vsm::saturating(decode_size - r.decoded),
			target_data,
			target_size);

		r.decoded += r2.decoded;
		r.encoded += r2.encoded;

		vsm_assert(r.decoded <= decode_size);
		vsm_assert(r.encoded <= encode_size || encode_data == nullptr);

		if (r2.ec != transcode_error{})
		{
			r.ec = r2.ec;
			break;
		}
	}
	return r;
}


template<native_character SourceChar, native_character TargetChar>
static transcode_result _transcode(
	SourceChar const* const decode_data,
	size_t const decode_size,
	TargetChar* const encode_data,
	size_t const encode_size)
{
	return transcode_win32(
		CP_UTF8,
		decode_data,
		decode_size,
		encode_data,
		encode_size);
}

template<character Char>
static transcode_result _transcode(
	Char const* const decode_data,
	size_t const decode_size,
	Char* const encode_data,
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
		memmove(encode_data, decode_data, size * sizeof(Char));
	}

	if (decode_size > encode_size)
	{
		r.ec = transcode_error::no_buffer_space;
	}

	return r;
}

template<character SourceChar, character TargetChar>
static transcode_result _transcode(
	SourceChar const* const decode_data,
	size_t const decode_size,
	TargetChar* const encode_data,
	size_t const encode_size)
{
	if constexpr (std::is_same_v<SourceChar, char32_t> || std::is_same_v<TargetChar, char32_t>)
	{
		// UTF32 transcoding cannot be implemented using the Windows transcoding APIs.
		return { transcode_error::unsupported_operation };
	}
	else
	{
		return _transcode(
			reinterpret_cast<native_character_t<SourceChar> const*>(decode_data),
			decode_size,
			reinterpret_cast<native_character_t<TargetChar>*>(encode_data),
			encode_size);
	}
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
