#include <allio/file_handle.hpp>

#include <allio/impl/linux/sync_filesystem_handle.hpp>
#include <allio/impl/sync_byte_io.hpp>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

vsm::result<void> file_handle_base::sync_impl(io::parameters_with_result<io::filesystem_open> const& args)
{
	return linux::sync_open<file_handle>(args);
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
