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

	// Scan the environment variables for a variable named '=X:', where X is the drive letter.
	static std::optional<path_section<wchar_t>> get_current_directory_on_drive(unique_lock_type const& lock, wchar_t const drive)
	{
		vsm_assert(L'A' <= drive && drive <= L'Z');

		vsm_assert(lock.owns_lock());
		win32::RTL_USER_PROCESS_PARAMETERS const& process_parameters = *NtCurrentPeb()->ProcessParameters;
		wchar_t const* environment = static_cast<wchar_t const*>(process_parameters.Environment);

		while (*environment)
		{
			wchar_t const* const beg = environment;
			wchar_t const* const end = beg + std::wcslen(beg);

			// Match "=X:=*".
			if (end - beg >= 4 && beg[0] == L'=' && beg[1] == drive && beg[2] == L':' && beg[3] == L'=')
			{
				//TODO: If the path has a relative component, open a directory handle.
				//      Do this because Win32 also tests for the validity of this path.
				//      Otherwise reject the path. Win32 falls back to drive root.
				return path_section{ beg + 4, end };
			}

			environment = end + 1;
		}

		return std::nullopt;
	}
};

} // namespace

vsm::result<kernel_path> win32::make_kernel_path(kernel_path_storage& storage, kernel_path_parameters const& args)
{
	return basic_kernel_path_converter<peb_context const>::make_path({}, storage, args);
}
