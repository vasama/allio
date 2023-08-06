#include <allio/section_handle.hpp>

using namespace allio;
using namespace allio::win32;

static vsm::result<ULONG> get_page_size_

vsm::result<void> detail::map_handle_base::sync_impl(io::parameters_with_result<io::section_create> const& args)
{
	section_handle& h = *args.handle;

	if (h)
	{
		return vsm::unexpected(error::handle_is_not_null);
	}

	HANDLE handle;

	NTSTATUS const status = NtCreateSection(
		&handle,
		SECTION_ALL_ACCESS, //TODO: Access

	)

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<nt_error>(status));
	}

	return {};
}
