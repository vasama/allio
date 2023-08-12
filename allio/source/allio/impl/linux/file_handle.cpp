#include <allio/file_handle.hpp>

#include <allio/impl/linux/filesystem_handle.hpp>
#include <allio/impl/sync_byte_io.hpp>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

vsm::result<void> file_handle_base::sync_impl(io::parameters_with_result<io::filesystem_open> const& args)
{
	file_handle& h = static_cast<file_handle&>(*args.handle);

	if (h)
	{
		return vsm::unexpected(error::handle_is_not_null);
	}

	vsm_try(h_fd, create_file(args.base, args.path, args));

	return initialize_platform_handle(
		h, vsm_move(h_fd),
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

vsm::result<void> file_handle_base::sync_impl(io::parameters_with_result<io::scatter_read_at> const& args)
{
	return args.produce(sync_scatter_read(static_cast<platform_handle const&>(*args.handle), args));
}

vsm::result<void> file_handle_base::sync_impl(io::parameters_with_result<io::gather_write_at> const& args)
{
	return args.produce(sync_gather_write(static_cast<platform_handle const&>(*args.handle), args));
}

//TODO: Some linux header defines file_handle...
//      Consider wrapping all sources in namespace allio?
allio_handle_implementation(allio::file_handle);
