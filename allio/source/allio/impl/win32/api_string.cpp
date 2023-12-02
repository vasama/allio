#include <allio/impl/win32/api_string.hpp>

using namespace allio;
using namespace allio::win32;

vsm::result<wchar_t const*> win32::make_api_string(
	api_string_storage& storage,
	any_string_view const string)
{
	if (string.encoding() == encoding::wide_execution_encoding && string.is_null_terminated())
	{
		return string.data<wchar_t>();
	}

	api_string_builder builder(storage, /* insert_null_terminator: */ true);
	vsm_try_void(api_string_builder::visit(string, vsm_lift_borrow(builder.push)));
	return builder.finalize().first;
}
