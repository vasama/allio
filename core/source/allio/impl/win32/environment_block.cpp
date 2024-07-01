#include <allio/impl/win32/environment_block.hpp>

using namespace allio;
using namespace allio::win32;

vsm::result<wchar_t*> win32::make_environment_block(
	api_string_storage& storage,
	any_string_span const environment)
{
	api_string_builder builder(storage, /* insert_null_terminator: */ true);
	for (any_string_view const variable : environment)
	{
		vsm_try_void(api_string_builder::visit(variable, [&](auto const string) -> vsm::result<void>
		{
			if (string.find('=') == string.npos)
			{
				return vsm::unexpected(error::invalid_argument);
			}

			vsm_try_void(builder.push(string));
			vsm_try_void(builder.push(std::wstring_view(L"\0", 1)));

			return {};
		}));
	}
	return builder.finalize().first;
}
