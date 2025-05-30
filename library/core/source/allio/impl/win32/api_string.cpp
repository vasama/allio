#include <allio/impl/win32/api_string.hpp>

#include <vsm/lift.hpp>

using namespace allio;
using namespace allio::win32;

static wchar_t const* get_wide_cstr(any_string_view const string)
{
	if (string.is_null_terminated())
	{
		switch (string.char_type())
		{
		default:
			break;

		case char_type::_wchar_t:
			return string.data<wchar_t>();

		case char_type::_char16_t:
			return reinterpret_cast<wchar_t const*>(string.data<char16_t>());
		}
	}

	return nullptr;
}

vsm::result<wchar_t const*> win32::make_api_string(
	api_string_storage& storage,
	any_string_view const string)
{
	if (wchar_t const* const c_str = get_wide_cstr(string))
	{
		return c_str;
	}

	api_string_builder builder(storage, /* insert_null_terminator: */ true);
	vsm_try_void(api_string_builder::visit(string, vsm_lift_borrow(builder.push)));
	return builder.finalize().first;
}
