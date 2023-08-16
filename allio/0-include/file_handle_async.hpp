#pragma once

#include <allio/file_handle.hpp>

#include <allio/async.hpp>

namespace allio {
namespace detail {

inline auto open_file_async_impl(multiplexer& multiplexer, filesystem_handle const* const base, path_view const path, file_handle::open_parameters const& args)
{
	return initialize_handle<file_handle>(multiplexer, [=, args = args](auto& h) mutable
	{
		args.multiplexable = true;
		return h.open_async(base, path, args);
	});
}

template<parameters<file_handle_base::open_parameters> Parameters>
basic_sender<io::filesystem_open> file_handle_base::open_async(input_path_view const path, Parameters const& args)
{
	return { *this, args, nullptr, path };
}

template<parameters<file_handle_base::open_parameters> Parameters>
basic_sender<io::filesystem_open> file_handle_base::open_async(filesystem_handle const& base, input_path_view const path, Parameters const& args)
{
	return { *this, args, &base, path };
}

template<parameters<file_handle_base::open_parameters> Parameters>
basic_sender<io::filesystem_open> file_handle_base::open_async(filesystem_handle const* const base, input_path_view const path, Parameters const& args)
{
	return { *this, args, base, path };
}

template<parameters<file_handle_base::byte_io_parameters> Parameters>
basic_sender<io::scatter_read_at> file_handle_base::read_at_async(file_size const offset, read_buffer const buffer, Parameters const& args) const
{
	return { *this, args, offset, buffer };
}

template<parameters<file_handle_base::byte_io_parameters> Parameters>
basic_sender<io::scatter_read_at> file_handle_base::read_at_async(file_size const offset, read_buffers const buffers, Parameters const& args) const
{
	return { *this, args, offset, buffers };
}

template<parameters<file_handle_base::byte_io_parameters> Parameters>
basic_sender<io::gather_write_at> file_handle_base::write_at_async(file_size const offset, write_buffer const buffer, Parameters const& args) const
{
	return { *this, args, offset, buffer };
}

template<parameters<file_handle_base::byte_io_parameters> Parameters>
basic_sender<io::gather_write_at> file_handle_base::write_at_async(file_size const offset, write_buffers const buffers, Parameters const& args) const
{
	return { *this, args, offset, buffers };
}

} // namespace detail


template<parameters<file_handle::open_parameters> Parameters = file_handle::open_parameters::interface>
auto open_file_async(multiplexer& multiplexer, path_view const path, Parameters const& args = {})
{
	return detail::open_file_async_impl(multiplexer, nullptr, path, args);
}

template<parameters<file_handle::open_parameters> Parameters = file_handle::open_parameters::interface>
auto open_file_async(multiplexer& multiplexer, filesystem_handle const& base, path_view const path, Parameters const& args = {})
{
	return detail::open_file_async_impl(multiplexer, &base, path, args);
}

} // namespace allio
