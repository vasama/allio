#include <allio/section_handle.hpp>

#include <allio/impl/linux/dup.hpp>
#include <allio/impl/linux/platform_handle.hpp>
#include <allio/linux/platform.hpp>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

vsm::result<void> section_handle_base::sync_impl(io::parameters_with_result<io::section_create> const& args)
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


	//TODO: Use some handle_ref<T> type which carries
	//      value category information to avoid dup on rvalues.
	//      This is probably useful in some other places as well.
	vsm_try(file, linux::dup(
		unwrap_handle(backing_file.get_platform_handle()),
		-1,
		O_CLOEXEC));


	return initialize_platform_handle(h, vsm_move(file),
		[&](native_platform_handle const handle)
		{
			return platform_handle::native_handle_type
			{
				handle::native_handle_type
				{
					handle::flags::not_null,
				},
				handle,
			};
		}
	);
}

allio_handle_implementation(section_handle);
