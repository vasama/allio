#include <allio/file_handle.hpp>

#include <allio/impl/sync_byte_io.hpp>
#include <allio/impl/win32/sync_filesystem_handle.hpp>

using namespace allio;
using namespace allio::win32;

result<void> detail::file_handle_base::sync_impl(io::parameters_with_result<io::filesystem_open> const& args)
{
	return sync_open<file_handle>(args, open_kind::file);
}

result<void> detail::file_handle_base::sync_impl(io::parameters_with_result<io::scatter_read_at> const& args)
{
	return args.produce(sync_scatter_read(static_cast<platform_handle const&>(*args.handle), args));
}

result<void> detail::file_handle_base::sync_impl(io::parameters_with_result<io::gather_write_at> const& args)
{
	return args.produce(sync_gather_write(static_cast<platform_handle const&>(*args.handle), args));
}

allio_HANDLE_IMPLEMENTATION(file_handle);
