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

namespace {

struct builder : api_string_builder
{
	using api_string_builder::api_string_builder;


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
	vsm::result<void> _push_argument(std::wstring_view const argument)
	{
		auto const info = parse_argument(argument);

		vsm_try_bind((out_beg, out_end), reserve_space(
			info.requires_quotes +
			argument.size() +
			info.escape_count +
			info.requires_quotes +
			/* space or null terminator */ 1));

		wchar_t* const arg_beg = out_beg + info.requires_quotes;
		wchar_t* const arg_end = out_end - info.requires_quotes - /* space or null terminator */ 1;
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

	vsm::result<void> _push_argument(std::string_view const argument)
	{
		vsm_try_bind((out_beg, out_end), transcode_temporary(argument));
		return _push_argument(std::wstring_view(out_beg, out_end));
	}

	vsm::result<void> push_argument(any_string_view const argument)
	{
		return visit(argument, vsm_lift_borrow(_push_argument));
	}
};

} // namespace

vsm::result<wchar_t*> win32::make_command_line(
	api_string_storage& storage,
	any_string_view const program,
	any_string_span const arguments)
{
	// The null terminator is inserted manually by replacing a space.
	builder builder(storage, /* insert_null_terminator: */ false);

	vsm_try_void(builder.push_argument(program));
	for (any_string_view const argument : arguments)
	{
		vsm_try_void(builder.push_argument(argument));
	}

	auto const [beg, end] = builder.finalize();

	vsm_assert(beg != end);
	vsm_assert(end[-1] == L' ');
	end[-1] = L'\0';

	return beg;
}
