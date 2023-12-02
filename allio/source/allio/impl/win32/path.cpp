#if 0
#include <allio/impl/win32/path.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::detail::path_impl;
using namespace allio::win32;

static bool is_dos_path(std::wstring_view const path)
{
	wchar_t const* const beg = path.data();
	wchar_t const* const end = path.data() + path.size();

	

	return path_impl::has_drive_letter(beg, end);
}

vsm::result<wchar_t const*> win32::make_path(
	api_string_storage& storage,
	std::wstring_view root_path
	any_string_view const path)
{
	if (root_path.empty() &&
		path.encoding() == encoding::wide_execution_encoding &&
		path.is_null_terminated())
	{
		auto const wide = path.view<wchar_t>();

		auto const beg = wide.data();
		auto const end = wide.data() + wide.size();

		if (wide.size() <= MAX_PATH)
		{
			return beg;
		}

		//TODO: Borrow DOS paths.
	}

	api_string_reverse_builder builder(
		storage,
		/* insert_null_terminator: */ true);

	bool is_relative = false;
	bool insert_unc_prefix = false;
	wchar_t drive_letter = L'\0';

	vsm_try_void(api_string_builder::visit(path, [&](auto const string) -> vsm::result<void>
	{
		vsm_try_void(builder.push(string));

		auto const beg = string.data();
		auto const end = string.data() + string.size();

		if (has_drive_letter(beg, end))
		{
			Char const* const sep = beg + 2;
			if (sep != end && is_separator(*sep))
			{
				is_relative = true;
				drive_letter = *beg;
			}
		}
		else
		{
			is_relative = find_root_name_end(beg, end) == beg;
		}

		return {};
	}));

	if (is_relative)
	{
		if (root_path.empty())
		{
			unique_peb_lock lock(true);

			if (drive_letter == L'\0')
			{
				auto const current = get_current_directory(lock);
			}
			else
			{
				if (auto const current = get_current_directory_on_drive(lock, drive_letter))
				{
				}
				else
				{
					root_path = 
				}
			}
		}

		vsm_try_void(push(root_path));
	}

	if (insert_unc_prefix)
	{
		vsm_try_void(builder.push(L"\\\\?\\"));
	}

	return builder.finalize();
}
#endif
