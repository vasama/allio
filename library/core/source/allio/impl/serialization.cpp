#include <allio/detail/serialization.hpp>

#include <allio/impl/error_encoding.hpp>

#include <vsm/array.hpp>

#include <charconv>

using namespace allio;
using namespace allio::detail;

namespace {

template<typename Char>
struct from_chars_result
{
	Char const* ptr;
	std::errc ec;
};

template<typename Char>
static uint8_t from_chars_digit(Char const x)
{
	if (static_cast<Char>('0') <= x && x <= static_cast<Char>('9'))
	{
		return static_cast<uint8_t>(x - static_cast<Char>('0'));
	}

	if (static_cast<Char>('a') <= x && x <= static_cast<Char>('f'))
	{
		return static_cast<uint8_t>(x - (static_cast<Char>('a') - 10));
	}

	if (static_cast<Char>('A') <= x && x <= static_cast<Char>('F'))
	{
		return static_cast<uint8_t>(x - (static_cast<Char>('A') - 10));
	}

	return static_cast<uint8_t>(-1);
}

template<typename Char, std::unsigned_integral Unsigned>
static from_chars_result<Char> from_chars_impl(
	Char const* pos,
	Char const* const end,
	Unsigned& value,
	Unsigned const max,
	Unsigned const radix)
{
	Unsigned accumulator = 0;

	for (; pos != end; ++pos)
	{
		if (*pos != static_cast<Char>('0'))
		{
			break;
		}
	}

	Unsigned const max_mul = max / radix;

	for (; pos != end; ++pos)
	{
		uint8_t const digit = from_chars_digit(*pos);

		if (digit >= radix)
		{
			break;
		}

		if (accumulator > max_mul || accumulator > max - digit)
		{
			return { pos, std::errc::result_out_of_range };
		}

		accumulator *= radix;
		accumulator += digit;
	}

	value = accumulator;
	return { pos };
}

template<typename Char, std::integral Integer>
static from_chars_result<Char> from_chars(
	Char const* pos,
	Char const* const end,
	Integer& value,
	int const radix = 10)
{
	vsm_assert(2 <= radix && radix <= 0x10); //PRECONDITION

	using unsigned_type = std::make_unsigned_t<Integer>;

	Char const* const beg = pos;

	bool is_negative = false;
	if constexpr (std::is_signed_v<Integer>)
	{
		if (pos != end && *pos == static_cast<Char>('-'))
		{
			++pos;
			is_negative = true;
		}
	}

	unsigned_type const max = std::is_unsigned_v<Integer>
		? std::numeric_limits<Integer>::max()
		: static_cast<unsigned_type>(std::numeric_limits<Integer>::max()) + is_negative;

	unsigned_type abs_value;
	auto const r = from_chars_impl(pos, end, abs_value, max, static_cast<unsigned_type>(radix));

	if (r.ec == static_cast<std::errc>(0))
	{
		if (r.ptr == pos)
		{
			return { beg, std::errc::invalid_argument };
		}

		if constexpr (std::is_signed_v<Integer>)
		{
			if (is_negative)
			{
				value = static_cast<Integer>(~abs_value + 1);
			}
			else
			{
				value = static_cast<Integer>(abs_value);
			}
		}
		else
		{
			value = abs_value;
		}
	}

	return r;
}


template<typename Char>
struct to_chars_result
{
	Char* ptr;
	std::errc ec;
};

static char to_chars_digit(uint8_t const x)
{
	return x < 10
		? static_cast<char>(static_cast<uint8_t>('0') + x)
		: static_cast<char>(static_cast<uint8_t>('a') - 10 + x);
}

template<typename Char, std::unsigned_integral Unsigned>
static to_chars_result<Char> to_chars_impl(
	Char* pos,
	Char* const end,
	Unsigned value,
	Unsigned const radix)
{
	Char* const beg = pos;

	while (value != 0)
	{
		if (pos == end)
		{
			return { end, std::errc::value_too_large };
		}

		Unsigned const digit = value % radix;
		value = value / radix;

		*pos++ = static_cast<Char>(to_chars_digit(static_cast<uint8_t>(digit)));
	}

	if (pos == beg)
	{
		if (pos == end)
		{
			return { end, std::errc::value_too_large };
		}

		*pos++ = static_cast<Char>('0');
	}
	else
	{
		std::reverse(beg, pos);
	}

	return { pos };
}

template<typename Char, std::integral Integer>
static to_chars_result<Char> to_chars(
	Char* beg,
	Char* const end,
	Integer const value,
	int const radix = 10)
{
	vsm_assert(2 <= radix && radix <= 0x10); //PRECONDITION

	using unsigned_type = std::make_unsigned_t<Integer>;
	unsigned_type unsigned_value = static_cast<unsigned_type>(value);

	if constexpr (std::is_signed_v<Integer>)
	{
		if (value < 0)
		{
			if (beg == end)
			{
				return { end, std::errc::value_too_large };
			}

			*beg++ = static_cast<Char>('-');
			unsigned_value = ~unsigned_value + 1;
		}
	}

	return to_chars_impl(beg, end, unsigned_value, static_cast<unsigned_type>(radix));
}


static size_t to_hex_uint64_size(uint64_t const value, bool const is_signed)
{
	char string[32];
	auto const r = std::to_chars(string, string + sizeof(string), value, 0x10);
	return static_cast<size_t>(r.ptr - string);
}

template<typename Char>
static to_chars_result<Char> to_hex_uint64(
	Char* const beg,
	Char* const end,
	uint64_t const value,
	bool const is_signed)
{
	return is_signed
		? to_chars(beg, end, static_cast<int64_t>(value), 0x10)
		: to_chars(beg, end, value, 0x10);
}

template<typename Char>
static from_chars_result<Char> from_hex_uint64(
	Char const* const beg,
	Char const* const end,
	uint64_t& value,
	bool const is_signed)
{
	if (is_signed)
	{
		int64_t signed_value;
		auto const r = from_chars(beg, end, signed_value, 0x10);
		value = static_cast<uint64_t>(signed_value);
		return r;
	}
	else
	{
		return from_chars(beg, end, value, 0x10);
	}
}


static_assert(
	std::endian::native == std::endian::big ||
	std::endian::native == std::endian::little);

using uint64_storage_t = vsm::array<uint8_t, sizeof(uint64_t)>;

static uint64_t read_integer(void const* const ptr, size_t const size)
{
	uint64_storage_t storage = {};

	if constexpr (std::endian::native == std::endian::little)
	{
		std::memcpy(storage.data(), ptr, size);
	}
	else
	{
		std::memcpy(storage.data() + sizeof(uint64_t) - size, ptr, size);
	}

	return std::bit_cast<uint64_t>(storage);
}

static uint64_t read_integer(void const* const ptr, size_t const size, bool const is_signed)
{
	uint64_t value = read_integer(ptr, size);

	if (is_signed)
	{
		size_t const shift = (sizeof(uint64_t) - size) * CHAR_BIT;
		value = static_cast<uint64_t>(static_cast<int64_t>(value << shift) >> shift);
	}

	return value;
}

static void write_integer(void* const ptr, size_t const size, uint64_t const value)
{
	auto const storage = std::bit_cast<uint64_storage_t>(value);

	if constexpr (std::endian::native == std::endian::little)
	{
		std::memcpy(ptr, storage.data(), size);
	}
	else
	{
		std::memcpy(ptr, storage.data() + sizeof(uint64_t) - size, size);
	}
}


class counting_serialization_context final : public serialization_context
{
	size_t m_size = 0;

public:
	size_t size() const
	{
		return m_size;
	}

private:
	vsm::result<void> _visit_header(std::string_view const id, uint32_t const version) override
	{
		m_size += id.size() + 1 + to_hex_uint64_size(version, /* is_signed: */ false);
		return {};
	}

	vsm::result<void> _visit_integer(
		void* const data,
		size_t const size,
		bool const is_signed) override
	{
		m_size += 1 + to_hex_uint64_size(read_integer(data, size, is_signed), is_signed);
		return {};
	}
};

template<typename Char>
class encoding_serialization_context final : public serialization_context
{
	Char* m_out_beg;
	Char* m_out_pos;
	Char* m_out_end;

public:
	explicit encoding_serialization_context(Char* const out_beg, Char* const out_end)
		: m_out_beg(out_beg)
		, m_out_pos(out_beg)
		, m_out_end(out_end)
	{
	}

	size_t size() const
	{
		return static_cast<size_t>(m_out_pos - m_out_beg);
	}

private:
	vsm::result<void> _visit_header(std::string_view const id, uint32_t const version) override
	{
		m_version = version;
		m_out_pos = std::copy(id.begin(), id.end(), m_out_pos);
		return visit(m_version);
	}

	vsm::result<void> _visit_integer(
		void* const data,
		size_t const size,
		bool const is_signed) override
	{
		*m_out_pos++ = static_cast<Char>(':');

		uint64_t const value = read_integer(data, size, is_signed);
		auto const [new_pos, error] = to_hex_uint64(m_out_pos, m_out_end, value, is_signed);

		vsm_assert(error == static_cast<std::errc>(0));
		m_out_pos = new_pos;

#if 0
		unsigned char const* const beg = static_cast<unsigned char const*>(data);
		unsigned char const* const end = beg + size;

		auto const r = m_encoder.encode<Char>(
			beg,
			end,
			static_cast<Char*>(m_out_pos));

		vsm_assert(r.in == end);
		m_out_pos = r.out;
#endif

		return {};
	}
};

template<typename Char>
class decoding_serialization_context final : public serialization_context
{
	Char const* m_src_pos;
	Char const* m_src_end;

public:
	explicit decoding_serialization_context(std::basic_string_view<Char> const string)
		: m_src_pos(string.data())
		, m_src_end(string.data() + string.size())
	{
	}

	vsm::result<void> finalize()
	{
		if (m_src_pos != m_src_end)
		{
			//TODO: Use a more specific error code.
			return vsm::unexpected(allio_error(error::invalid_argument));
		}

		return {};
	}

private:
	vsm::result<void> _visit_header(std::string_view const id, uint32_t const version) override
	{
		auto const colon_pos = std::find(m_src_pos, m_src_end, static_cast<Char>(':'));

		if (!std::equal(m_src_pos, colon_pos, id.begin(), id.end()))
		{
			//TODO: Use a more specific error code.
			return vsm::unexpected(allio_error(error::invalid_argument));
		}

		m_src_pos = colon_pos;
		vsm_try_void(visit(m_version));

		if (m_version > version)
		{
			//TODO: Use a more specific error code.
			return vsm::unexpected(allio_error(error::invalid_argument));
		}

		return {};
	}

	vsm::result<void> _visit_integer(
		void* const data,
		size_t const size,
		bool const is_signed) override
	{
		if (m_src_pos == m_src_end || *m_src_pos++ != static_cast<Char>(':'))
		{
			//TODO: Use a more specific error code.
			return vsm::unexpected(allio_error(error::invalid_argument));
		}

		uint64_t value;
		auto const [new_pos, error] = from_hex_uint64(m_src_pos, m_src_end, value, is_signed);

		if (error != static_cast<std::errc>(0))
		{
			//TODO: Use a more specific error code.
			return vsm::unexpected(allio_error(error::invalid_argument));
		}

		size_t const shift = (sizeof(uint64_t) - size) * CHAR_BIT;
		uint64_t const truncated_value = is_signed
			? static_cast<uint64_t>(static_cast<int64_t>(value << shift) >> shift)
			: (value << shift) >> shift;

		if (value != truncated_value)
		{
			//TODO: Use a more specific error code.
			return vsm::unexpected(allio_error(error::invalid_argument));
		}

		write_integer(data, size, value);
		m_src_pos = new_pos;

#if 0
		unsigned char* const out_beg = static_cast<unsigned char*>(data);
		unsigned char* const out_end = out_beg + size;

		auto const r = m_decoder.decode<unsigned char>(
			m_src_pos,
			m_src_end,
			const_cast<unsigned char*>(out_beg),
			const_cast<unsigned char*>(out_end));

		if (r.out != out_end)
		{
			//TODO: Use a more specific error code.
			return vsm::unexpected(allio_error(error::invalid_argument));
		}

		m_src_pos = r.in;
#endif

		return {};
	}
};

} // namespace

static size_t get_handle_encoded_size(
	native_handle<object_t> const& h,
	serialize_callback_t* const serialize)
{
	counting_serialization_context serializer;
	vsm_verify(serialize(const_cast<native_handle<object_t>&>(h), serializer));
	return serializer.size();
}

vsm::result<size_t> detail::_encode_handle(
	native_handle<object_t> const& h,
	any_string_buffer const buffer,
	serialize_callback_t* const serialize)
{
	size_t const encoded_size = get_handle_encoded_size(h, serialize);
	return buffer.visit([&]<typename Char>(string_buffer<Char> const buffer) -> vsm::result<size_t>
	{
		vsm_try(string, buffer.resize(encoded_size));

		encoding_serialization_context<Char> serializer(
			string.data(),
			string.data() + string.size());

		vsm_verify(serialize(const_cast<native_handle<object_t>&>(h), serializer));
		vsm_assert(serializer.size() == encoded_size);

		return encoded_size;
	});
}

template<typename Char>
static vsm::result<void> decode_handle_impl(
	native_handle<object_t>& h,
	std::basic_string_view<Char> const string,
	serialize_callback_t* const serialize)
{
	decoding_serialization_context<Char> serializer(string);
	vsm_try_void(serialize(h, serializer));
	return serializer.finalize();
}

static vsm::result<void> decode_handle_impl(
	native_handle<object_t>& h,
	detail::string_length_out_of_range_t,
	serialize_callback_t* const serialize)
{
	return vsm::unexpected(allio_error(error::invalid_argument));
}

vsm::result<void> detail::_decode_handle(
	native_handle<object_t>& h,
	any_string_view const string,
	serialize_callback_t* const serialize)
{
	return string.visit([&](auto const string)
	{
		return ::decode_handle_impl(h, string, serialize);
	});
}


#if 0
namespace {

enum class serialization_state
{
	visiting,
	encoding,
	decoding,
};

struct _serialization_context : serialization_context
{
	serialization_state m_state;
	encoding m_string_data_type;

	union
	{
		size_t m_visiting_size;

		struct
		{
			vsm::base64_encoder m_encoder;
			void* m_encoder_out_pos;
		};

		struct
		{
			vsm::base64_decoder m_decoder;
			void const* m_decoder_src_pos;
			void const* m_decoder_src_end;
		};
	};

	_serialization_context()
	{
	}
};


} // namespace

template<typename Char>
static vsm::result<void> _visit(
	_serialization_context& context,
	void const* const data,
	size_t const size)
{
	if (context.m_state == serialization_state::encoding)
	{
		unsigned char const* const beg = static_cast<unsigned char const*>(data);
		unsigned char const* const end = beg + size;

		auto const r = context.m_encoder.encode<Char>(
			beg,
			end,
			static_cast<Char*>(context.m_encoder_out_pos));

		vsm_assert(r.in == end);

		context.m_encoder_out_pos = r.out;
	}
	else
	{
		Char const* const pos = static_cast<Char const*>(context.m_decoder_src_pos);
		Char const* const end = static_cast<Char const*>(context.m_decoder_src_end);

		unsigned char const* const out_beg = static_cast<unsigned char const*>(data);
		unsigned char const* const out_end = out_beg + size;

		auto const r = context.m_decoder.decode<unsigned char>(
			pos,
			end,
			const_cast<unsigned char*>(out_beg),
			const_cast<unsigned char*>(out_end));

		if (r.out != out_end)
		{
			//TODO: Use a more specific error code.
			return vsm::unexpected(allio_error(error::invalid_argument));
		}

		context.m_decoder_src_pos = r.in;
	}

	return {};
}

vsm::result<void> serialization_context::_visit(void const* const data, size_t const size)
{
	auto& context = static_cast<_serialization_context&>(*this);

	if (context.m_state == serialization_state::visiting)
	{
		context.m_visiting_size += size;
	}
	else
	{
		switch (context.m_string_data_type)
		{
		case encoding::narrow_execution_encoding:
			return ::_visit<char>(context, data, size);

		case encoding::wide_execution_encoding:
			return ::_visit<wchar_t>(context, data, size);

		case encoding::utf8:
			return ::_visit<char8_t>(context, data, size);

		case encoding::utf16:
			return ::_visit<char16_t>(context, data, size);

		case encoding::utf32:
			return ::_visit<char32_t>(context, data, size);
		}
	}

	return {};
}


template<typename Char>
static vsm::result<size_t> encode_impl(
	serialize_function_type* const serialize,
	native_handle<object_t> const& h,
	size_t const required_size,
	string_buffer<Char> const buffer)
{
	size_t const base64_size = vsm::base64_encoded_size(required_size);
	vsm_try(string, buffer.resize(base64_size));

	_serialization_context context;
	context.m_state = serialization_state::encoding;
	context.m_string_data_type = detail::encoding_of<Char>;
	context.m_encoder = vsm::base64_encoder();
	context.m_encoder_out_pos = string.data();

	vsm_verify(serialize(context, const_cast<native_handle<object_t>&>(h)));
	auto const r = context.m_encoder.finalize<Char>(static_cast<Char*>(context.m_encoder_out_pos));

	vsm_assert(r.completed);
	vsm_assert(static_cast<size_t>(r.out - string.data()) == base64_size);

	return base64_size;
}

vsm::result<size_t> detail::_encode_handle(
	serialize_function_type* const serialize,
	native_handle<object_t> const& h,
	any_string_buffer const buffer)
{
	counting_serialization_context serialization_context;
	

	_serialization_context context;
	context.m_state = serialization_state::visiting;
	context.m_visiting_size = 0;

	vsm_verify(serialize(context, const_cast<native_handle<object_t>&>(h)));

	return buffer.visit([&](auto const buffer)
	{
		return ::encode_impl(serialize, h, context.m_visiting_size, buffer);
	});
}


template<typename Char>
static vsm::result<void> decode_impl(
	serialize_function_type* const serialize,
	native_handle<object_t>& h,
	std::basic_string_view<Char> const string)
{
	_serialization_context context;
	context.m_state = serialization_state::decoding;
	context.m_string_data_type = encoding_of<Char>;
	context.m_decoder = vsm::base64_decoder();
	context.m_decoder_src_pos = string.data();
	context.m_decoder_src_end = string.data() + string.size();

	vsm_try_void(serialize(context, h));

	if (context.m_decoder_src_pos != context.m_decoder_src_end ||
		!context.m_decoder.is_valid_final_state())
	{
		//TODO: Use a more specific error code.
		return vsm::unexpected(allio_error(error::invalid_argument));
	}

	return {};
}

static vsm::result<void> decode_impl(
	serialize_function_type* const serialize,
	native_handle<object_t>& h,
	detail::string_length_out_of_range_t)
{
	return vsm::unexpected(allio_error(error::invalid_argument));
}

vsm::result<void> detail::_decode_handle(
	serialize_function_type* const serialize,
	native_handle<object_t>& h,
	any_string_view const string)
{
	return string.visit([&](auto const string)
	{
		return ::decode_impl(serialize, h, string);
	});
}
#endif
