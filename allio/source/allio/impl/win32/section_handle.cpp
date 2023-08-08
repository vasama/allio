#include <allio/section_handle.hpp>

#include <vsm/out_resource.hpp>

#include <allio/impl/win32/kernel.hpp>

#include <win32.h>

using namespace allio;
using namespace allio::win32;

vsm::result<void> detail::section_handle_base::sync_impl(io::parameters_with_result<io::section_create> const& args)
{
	section_handle& h = *args.handle;

	if (h)
	{
		return vsm::unexpected(error::handle_is_not_null);
	}


	if (args.backing_file == nullptr)
	{
		return vsm::unexpected(error::invalid_argument);
	}

	file_handle const& backing_file = *args.backing_file;

	if (!backing_file)
	{
		return vsm::unexpected(error::invalid_argument);
	}

	LARGE_INTEGER const maximum_size =
	{
		.QuadPart = args.maximum_size,
	};


	unique_handle section;

	NTSTATUS const status = NtCreateSection(
		out_resource(section),
		SECTION_ALL_ACCESS, //TODO: Access
		nullptr, // ObjectAttributes
		&maximum_size,
		0, //TODO: Page protection
		0, //TODO: Allocation attributes
		unwrap_handle(backing_file.get_platform_handle()));

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<nt_error>(status));
	}


	return initialize_platform_handle(h, vsm_move(section),
		[&](native_platform_handle const handle)
		{
			return platform_handle::native_handle_type
			{
				handle::native_handle_type
				{
					handle::flags::not_null,
				},
				wrap_handle(handle),
			}
		}
	);
}
