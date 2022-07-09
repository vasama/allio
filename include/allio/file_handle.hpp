#pragma once

#include <allio/async_fwd.hpp>
#include <allio/detail/api.hpp>
#include <allio/byte_io.hpp>
#include <allio/filesystem_handle.hpp>
#include <allio/multiplexer.hpp>

namespace allio {

namespace detail {

class file_handle_base : public filesystem_handle
{
public:
	using async_operations = type_list_cat<
		filesystem_handle::async_operations,
		io::random_access_scatter_gather
	>;

	using filesystem_handle::filesystem_handle;

	result<native_handle_type> release_native_handle();
	result<void> set_native_handle(native_handle_type handle);

	result<void> open(path_view path, file_parameters const& args = {});
	result<void> open(filesystem_handle const& base, path_view path, file_parameters const& args = {});

	basic_sender<io::open_file> open_async(path_view path, file_parameters const& args = {});
	basic_sender<io::open_file> open_async(filesystem_handle const& base, path_view path, file_parameters const& args = {});

	result<size_t> read_at(file_offset offset, read_buffers buffers);
	result<size_t> read_at(file_offset const offset, read_buffer const buffer)
	{
		return read_at(offset, read_buffers(&buffer, 1));
	}

	basic_sender<io::scatter_read_at> read_at_async(file_offset offset, read_buffer buffer);
	basic_sender<io::scatter_read_at> read_at_async(file_offset offset, read_buffers buffers);

	result<size_t> write_at(file_offset offset, write_buffers buffers);
	result<size_t> write_at(file_offset const offset, write_buffer const buffer)
	{
		return write_at(offset, write_buffers(&buffer, 1));
	}

	basic_sender<io::gather_write_at> write_at_async(file_offset offset, write_buffer buffer);
	basic_sender<io::gather_write_at> write_at_async(file_offset offset, write_buffers buffers);

private:
	result<void> open(filesystem_handle const* base, path_view path, file_parameters const& args);
	result<void> open_sync(filesystem_handle const* base, path_view path, file_parameters const& args);

	result<size_t> read_at_sync(file_offset offset, read_buffers buffers);
	result<size_t> write_at_sync(file_offset offset, write_buffers buffers);
};

} // namespace detail

using file_handle = final_handle<detail::file_handle_base>;
allio_API extern allio_TYPE_ID(file_handle);

result<file_handle> open_file(path_view path, file_parameters const& args = {});
result<file_handle> open_file(filesystem_handle const& base, path_view path, file_parameters const& args = {});

result<file_handle> open_unique_file(filesystem_handle const& base, file_parameters const& args = {});
result<file_handle> open_temporary_file(path_view relative_path, file_parameters const& args = {});

result<file_handle> open_anonymous_file(file_parameters const& args = {});
result<file_handle> open_anonymous_file(filesystem_handle const& base, file_parameters const& args = {});

} // namespace allio
