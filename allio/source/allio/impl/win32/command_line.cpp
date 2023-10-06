#include <allio/impl/win32/command_line.hpp>

#include <allio/error.hpp>
#include <allio/impl/transcode.hpp>

#include <vsm/lift.hpp>

#include <algorithm>

using namespace allio;
using namespace allio::win32;

static bool is_wide_encoding(encoding const e)
{
	return e == encoding::narrow_execution_encoding || e == encoding::utf16;
}

class win32::command_line_builder
{
	static constexpr size_t max_command_line_size = 0x7ffe;

	// Reserve one extra character for the null terminator.
	static constexpr size_t dynamic_buffer_size = max_command_line_size + 1;

	command_line_storage& m_storage;

	wchar_t* m_buffer_beg;
	wchar_t* m_buffer_pos;
	wchar_t* m_buffer_end;

public:
	explicit command_line_builder(command_line_storage& storage)
		: m_storage(storage)
	{
	}


	vsm::result<void> allocate_dynamic_buffer()
	{
		wchar_t* const new_buffer_beg = new (std::nothrow) wchar_t[dynamic_buffer_size];
		if (new_buffer_beg == nullptr)
		{
			return vsm::unexpected(error::not_enough_memory);
		}
		m_storage.m_dynamic.reset(new_buffer_beg);

		size_t const old_data_size = m_buffer_pos - m_buffer_beg;
		memcpy(new_buffer_beg, m_buffer_beg, old_data_size * sizeof(wchar_t));

		m_buffer_beg = new_buffer_beg;
		m_buffer_pos = new_buffer_beg + old_data_size;
		m_buffer_end = new_buffer_beg + dynamic_buffer_size;

		return {};
	}

	vsm::result<std::pair<wchar_t*, wchar_t*>> reserve(size_t const size)
	{
		if (size > static_cast<size_t>(m_buffer_end - m_buffer_pos))
		{
			size_t const old_data_size = m_buffer_pos - m_buffer_beg;

			if (old_data_size + size > dynamic_buffer_size)
			{
				return vsm::unexpected(error::command_line_too_long);
			}

			vsm_try_void(allocate_dynamic_buffer());
		}

		wchar_t* const beg = m_buffer_pos;
		wchar_t* const end = m_buffer_pos + size;
		m_buffer_pos = end;

		return std::pair<wchar_t*, wchar_t*>{ beg, end };
	}


	struct argument_info
	{
		/// @brief Whether the string requires quotes around it.
		bool requires_quotes;
		
		/// @brief Number of escape characters required.
		size_t escape_count;
	};

	static argument_info parse_argument(std::wstring_view const argument)
	{
		if (argument.empty())
		{
			return
			{
				.requires_quotes = true,
			};
		}

		argument_info r = {};
		for (wchar_t const character : argument)
		{
			switch (character)
			{
			case L' ':
				r.requires_quotes = true;
				break;

			case L'\\':
			case L'\"':
				++r.escape_count;
				break;
			}
		}
		return r;
	}


	static void copy_and_escape(
		wchar_t const* const beg,
		wchar_t const* end,
		wchar_t* const out_beg,
		wchar_t* out_end,
		size_t const escape_count)
	{
		if (escape_count == 0)
		{
			// The input and output have the same size.
			vsm_assert((out_end - out_beg) == (end - beg));

			// If no escaping is required, we may still need to copy or shift
			// the input string. Use memmove to shift the string after transcoding.
			if (beg != end && beg != out_beg)
			{
				memmove(out_beg, beg, (end - beg) * sizeof(wchar_t));
			}
		}
		else
		{
			// The input and output differ in size by the number of escapes.
			vsm_assert((out_end - out_beg) - (end - beg) == escape_count);

			while (beg != end)
			{
				wchar_t const character = *--end;
	
				vsm_assert(out_beg != out_end);
				*--out_end = character;
	
				switch (character)
				{
				case L'\\':
				case L'\"':
					vsm_assert(out_beg != out_end);
					*--out_end = L'\\';
				}
			}

			// The number of escape characters was correct.
			vsm_assert(out_beg == out_end);
		}
	}


	// The argument may be stored in the output buffer after transcoding.
	vsm::result<void> push_argument(std::wstring_view const argument)
	{
		auto const info = parse_argument(argument);

		vsm_try_bind((out_beg, out_end), reserve(
			info.requires_quotes +
			argument.size() +
			info.escape_count +
			info.requires_quotes +
			/* space */ 1));

		wchar_t* const arg_beg = out_beg + info.requires_quotes;
		wchar_t* const arg_end = out_end - info.requires_quotes - /* space */ 1;
		vsm_assert(arg_end - arg_beg == argument.size() + info.escape_count);

		copy_and_escape(
			argument.data(),
			argument.data() + argument.size(),
			arg_beg,
			arg_end,
			info.escape_count);

		// Delay quoting until after the copy. Otherwise the first
		// character of a transcoded argument will get clobbered.
		if (info.requires_quotes)
		{
			*out_beg = L'"';
			*arg_end = L'"';
		}

		// Insert a space after each argument.
		// The last space will be replaced by the null terminator.
		out_end[-1] = L' ';

		return {};
	}

	vsm::result<void> push_argument(std::string_view const argument)
	{
		auto const r1 = transcode<wchar_t>(
			argument,
			std::span(m_buffer_pos, m_buffer_end));

		size_t encoded = r1.encoded;

		if (r1.ec != transcode_error{})
		{
			if (r1.ec != transcode_error::no_buffer_space)
			{
				return vsm::unexpected(error::invalid_encoding);
			}

			if (m_storage.m_dynamic != nullptr)
			{
				return vsm::unexpected(error::command_line_too_long);
			}


			auto const r2 = transcode_size<wchar_t>(
				argument.substr(r1.decoded),
				max_command_line_size - r1.encoded - (m_buffer_pos - m_buffer_beg));

			if (r2.ec != transcode_error{})
			{
				return r2.ec == transcode_error::no_buffer_space
					? vsm::unexpected(error::command_line_too_long)
					: vsm::unexpected(error::invalid_encoding);
			}

			encoded += r2.encoded;


			vsm_try_void(allocate_dynamic_buffer());

			transcode_unchecked<wchar_t>(
				argument.substr(r1.decoded),
				std::span(m_buffer_pos + r1.encoded, m_buffer_end));
		}

		return push_argument(std::wstring_view(m_buffer_pos, encoded));
	}

	vsm::result<void> push_argument(std::basic_string_view<char8_t> const argument)
	{
		return push_argument(std::basic_string_view<char>(
			reinterpret_cast<char const*>(argument.data()),
			argument.size()));
	}

	vsm::result<void> push_argument(std::basic_string_view<char16_t> const argument)
	{
		//TODO: This is technically UB.
		return push_argument(std::basic_string_view<wchar_t>(
			reinterpret_cast<wchar_t const*>(argument.data()),
			argument.size()));
	}

	vsm::result<void> push_argument(std::basic_string_view<char32_t>)
	{
		return vsm::unexpected(error::unsupported_encoding);
	}

	vsm::result<void> push_argument(detail::string_length_out_of_range_t)
	{
		return vsm::unexpected(error::command_line_too_long);
	}

	vsm::result<void> push_argument(any_string_view const argument)
	{
		return argument.visit(vsm_lift_borrow(push_argument));
	}


	vsm::result<wchar_t*> make_command_line(
		any_string_view const program,
		any_string_span const arguments)
	{
		auto const set_buffer = [&](wchar_t* const beg, size_t const size)
		{
			m_buffer_beg = beg;
			m_buffer_pos = beg;
			m_buffer_end = beg + size;
		};

		if (m_storage.m_dynamic != nullptr)
		{
			set_buffer(m_storage.m_dynamic.get(), dynamic_buffer_size);
		}
		else
		{
			set_buffer(m_storage.m_static, std::size(m_storage.m_static));
		}

		vsm_try_void(push_argument(program));
		for (any_string_view const argument : arguments)
		{
			vsm_try_void(push_argument(argument));
		}

		// Replace the last space with the null terminator.
		vsm_assert(m_buffer_beg != m_buffer_end);
		vsm_assert(m_buffer_pos[-1] == L' ');
		m_buffer_pos[-1] = L'\0';

		return m_buffer_beg;
	}
};

vsm::result<wchar_t*> win32::make_command_line(
	command_line_storage& storage,
	any_string_view const program,
	any_string_span const arguments)
{
	return command_line_builder(storage).make_command_line(program, arguments);
}
