#include <allio/impl/win32/kernel_path_impl.hpp>

#include <allio/impl/win32/kernel.hpp>

using namespace allio;
using namespace allio::win32;
using namespace detail::kernel_path_impl;

namespace {

struct peb_context
{
	using handle_type = HANDLE;
	using unique_lock_type = unique_peb_lock;

	static unique_lock_type lock()
	{
		return unique_lock_type(true);
	}

	static current_directory<peb_context> get_current_directory(unique_lock_type const& lock)
	{
		vsm_assert(lock.owns_lock());
		win32::RTL_USER_PROCESS_PARAMETERS const& process_parameters = *NtCurrentPeb()->ProcessParameters;

		wchar_t const* const beg = process_parameters.CurrentDirectoryPath.Buffer;
		wchar_t const* const end = beg + process_parameters.CurrentDirectoryPath.Length / sizeof(wchar_t);
		return { process_parameters.CurrentDirectoryHandle, { beg, end } };
	}

	static std::optional<path_section<wchar_t>> get_current_directory_on_drive(unique_lock_type const& lock, wchar_t const drive)
	{
		vsm_assert(L'A' <= drive && drive <= L'Z');

		vsm_assert(lock.owns_lock());
		win32::RTL_USER_PROCESS_PARAMETERS const& process_parameters = *NtCurrentPeb()->ProcessParameters;
		wchar_t const* environment = static_cast<wchar_t const*>(process_parameters.Environment);

		while (*environment)
		{
			wchar_t const* const var_beg = environment;
			wchar_t const* const var_end = std::wcschr(environment, L'=');
			wchar_t const* const val_end = var_beg + std::wcslen(var_end);

			if (var_end != nullptr && var_end - var_beg == 3 &&
				var_beg[0] == L'=' && var_beg[1] == drive && var_beg[2] == L':')
			{
				return path_section{ var_end + 1, val_end };
			}

			environment = val_end + 1;
		}

		return std::nullopt;
	}
};

} // namespace

vsm::result<kernel_path> win32::make_kernel_path(kernel_path_storage& storage, kernel_path_parameters const& args)
{
	return basic_kernel_path_converter<peb_context const>::make_path({}, storage, args);
}
