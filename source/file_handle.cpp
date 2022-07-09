#include <allio/file_handle.hpp>

using namespace allio;

result<detail::file_handle_base::native_handle_type> detail::file_handle_base::release_native_handle()
{
	return filesystem_handle::release_native_handle();
}

result<void> detail::file_handle_base::set_native_handle(native_handle_type const handle)
{
	return filesystem_handle::set_native_handle(handle);
}

result<void> detail::file_handle_base::open(path_view const path, file_parameters const& args)
{
	return open(nullptr, path, args);
}

result<void> detail::file_handle_base::open(filesystem_handle const& base, path_view const path, file_parameters const& args)
{
	return open(&base, path, args);
}

result<void> detail::file_handle_base::open(filesystem_handle const* const base, path_view const path, file_parameters const& args)
{
	if (*this)
	{
		return allio_ERROR(make_error_code(std::errc::bad_file_descriptor)); //TODO: use better error
	}

	file_parameters args_copy = args;
	if (multiplexer* const multiplexer = get_multiplexer())
	{
		args_copy.handle_flags |= flags::multiplexable;

		if (!is_synchronous<io::open_file>(*this))
		{
			return block<io::open_file>(*this, base, path, args_copy);
		}
	}

	return open_sync(base, path, args_copy);
}

result<size_t> detail::file_handle_base::read_at(file_offset const offset, read_buffers const buffers)
{
	if (!*this)
	{
		return allio_ERROR(make_error_code(std::errc::bad_file_descriptor));
	}

	if (multiplexer* const multiplexer = get_multiplexer())
	{
		return block<io::scatter_read_at>(*this, offset, buffers);
	}

	return read_at_sync(offset, buffers);
}

result<size_t> detail::file_handle_base::write_at(file_offset const offset, write_buffers const buffers)
{
	if (!*this)
	{
		return allio_ERROR(make_error_code(std::errc::bad_file_descriptor));
	}

	if (multiplexer* const multiplexer = get_multiplexer())
	{
		return block<io::gather_write_at>(*this, offset, buffers);
	}

	return write_at_sync(offset, buffers);
}

allio_TYPE_ID(file_handle);
