#pragma once

#include <allio/file_handle.hpp>

#include <allio/async.hpp>

#include <unifex/defer.hpp>
#include <unifex/just.hpp>
#include <unifex/let_value_with.hpp>
#include <unifex/sequence.hpp>

namespace allio {
namespace detail {

inline auto open_file_async_impl(multiplexer& multiplexer, filesystem_handle const* const base, path_view const path, file_handle::open_parameters const& args)
{
	struct context
	{
		file_handle handle;
		allio::multiplexer* multiplexer;
		filesystem_handle const* base;
		input_path_view path;
		file_handle::open_parameters args;
	};

	return error_into_except(unifex::let_value_with(
		[ctx = context{ file_handle(), &multiplexer, base, path, args }]() mutable -> context&
		{
			ctx.args.multiplexable = true;
			return ctx;
		},
		[](context& ctx)
		{
			return unifex::sequence(
				unifex::defer([&] { return result_into_error(unifex::just(ctx.handle.set_multiplexer(ctx.multiplexer))); }),
				unifex::defer([&] { return basic_sender<io::filesystem_open>(ctx.handle, ctx.args, ctx.base, ctx.path); }),
				unifex::defer([&] { return unifex::just(static_cast<decltype(context::handle)&&>(ctx.handle)); })
			);
		}
	));
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
basic_sender<io::scatter_read_at> file_handle_base::read_at_async(file_offset const offset, read_buffer const buffer, Parameters const& args) const
{
	return { *this, args, offset, buffer };
}

template<parameters<file_handle_base::byte_io_parameters> Parameters>
basic_sender<io::scatter_read_at> file_handle_base::read_at_async(file_offset const offset, read_buffers const buffers, Parameters const& args) const
{
	return { *this, args, offset, buffers };
}

template<parameters<file_handle_base::byte_io_parameters> Parameters>
basic_sender<io::gather_write_at> file_handle_base::write_at_async(file_offset const offset, write_buffer const buffer, Parameters const& args) const
{
	return { *this, args, offset, buffer };
}

template<parameters<file_handle_base::byte_io_parameters> Parameters>
basic_sender<io::gather_write_at> file_handle_base::write_at_async(file_offset const offset, write_buffers const buffers, Parameters const& args) const
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
