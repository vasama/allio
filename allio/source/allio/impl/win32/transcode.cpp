#define WIN32_NLS

#include <allio/impl/transcode.hpp>

#include <allio/impl/win32/error.hpp>

#include <vsm/assert.h>

#include <win32.h>

using namespace allio;
using namespace allio::detail;

template<typename SourceChar, typename TargetChar>
static int _transcode_win32(
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

template<typename SourceChar, typename TargetChar>
static transcode_result _transcode(
	std::basic_string_view<SourceChar> const decode_buffer,
	TargetChar* const encode_buffer_data,
	size_t const encode_buffer_size,
	UINT const code_page,
	bool const do_encode)
{
	transcode_result r = {};

	if (!decode_buffer.empty())
	{
		if (do_encode && encode_buffer_size == 0)
		{
			r.ec = transcode_error::no_buffer_space;
		}
		else
		{
			int const size = _transcode_win32(
				code_page,
				decode_buffer.data(),
				decode_buffer.size(),
				encode_buffer_data,
				encode_buffer_size);

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
				r.decoded = decode_buffer.size();
				r.encoded = static_cast<size_t>(size);
			}
		}
	}

	return r;
}

template<character TargetChar, character SourceChar>
transcode_result allio::transcode_size(
	std::basic_string_view<SourceChar> const decode_buffer,
	size_t const max_encoded_size)
{
	transcode_result r = _transcode(
		decode_buffer,
		static_cast<TargetChar*>(nullptr),
		0,
		CP_UTF8,
		false);

	if (r.ec == transcode_error{} && r.encoded > max_encoded_size)
	{
		r.ec = transcode_error::no_buffer_space;
	}

	return r;
}

template<character TargetChar, character SourceChar>
transcode_result allio::transcode(
	std::basic_string_view<SourceChar> const decode_buffer,
	std::span<TargetChar> const encode_buffer)
{
	return _transcode(
		decode_buffer,
		encode_buffer.data(),
		encode_buffer.size(),
		CP_UTF8,
		true);
}

#define allio_detail_transcode_instance(S, T) \
	template transcode_result allio::transcode_size<T, S>(std::basic_string_view<S>, size_t); \
	template transcode_result allio::transcode<T, S>(std::basic_string_view<S>, std::span<T>)

allio_detail_transcode_instance(char, wchar_t);
allio_detail_transcode_instance(wchar_t, char);

allio_detail_transcode_instance(char, char16_t);
allio_detail_transcode_instance(char16_t, char);

allio_detail_transcode_instance(char8_t, wchar_t);
allio_detail_transcode_instance(wchar_t, char8_t);

allio_detail_transcode_instance(char8_t, char16_t);
allio_detail_transcode_instance(char16_t, char8_t);
