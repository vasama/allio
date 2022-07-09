#pragma once

#include <allio/file_handle.hpp>

#include <allio/async.hpp>

#include <unifex/defer.hpp>
#include <unifex/just.hpp>
#include <unifex/let_value_with.hpp>
#include <unifex/sequence.hpp>

namespace allio {
namespace detail {

inline auto open_file_async_impl(multiplexer& multiplexer, filesystem_handle const* const base, path_view const path, file_parameters const& args)
{
	struct context
	{
		file_handle handle;
		allio::multiplexer* multiplexer;
		filesystem_handle const* base;
		path_view path;
		file_parameters args;
	};

	return error_into_except(unifex::let_value_with(
		[ctx = context{ file_handle(), &multiplexer, base, path, args }]() mutable -> context&
		{
			ctx.args.handle_flags |= flags::multiplexable;
			return ctx;
		},
		[](context& ctx)
		{
			return unifex::sequence(
				unifex::defer([&] { return result_into_error(unifex::just(ctx.handle.set_multiplexer(ctx.multiplexer))); }),
				unifex::defer([&] { return basic_sender<io::open_file>(ctx.handle, ctx.base, ctx.path, ctx.args); }),
				unifex::defer([&] { return unifex::just(static_cast<decltype(context::handle)&&>(ctx.handle)); })
			);
		}
	));
}

} // namespace detail

inline basic_sender<io::open_file> detail::file_handle_base::open_async(path_view const path, file_parameters const& args)
{
	return { *this, nullptr, path, args };
}

inline basic_sender<io::open_file> detail::file_handle_base::open_async(filesystem_handle const& base, path_view const path, file_parameters const& args)
{
	return { *this, &base, path, args };
}

inline basic_sender<io::scatter_read_at> detail::file_handle_base::read_at_async(file_offset const offset, read_buffer const buffer)
{
	return { *this, offset, buffer };
}

inline basic_sender<io::scatter_read_at> detail::file_handle_base::read_at_async(file_offset const offset, read_buffers const buffers)
{
	return { *this, offset, buffers };
}

inline basic_sender<io::gather_write_at> detail::file_handle_base::write_at_async(file_offset const offset, write_buffer const buffer)
{
	return { *this, offset, buffer };
}

inline basic_sender<io::gather_write_at> detail::file_handle_base::write_at_async(file_offset const offset, write_buffers const buffers)
{
	return { *this, offset, buffers };
}


inline auto open_file_async(multiplexer& multiplexer, path_view const path, file_parameters const& args = {})
{
	return detail::open_file_async_impl(multiplexer, nullptr, path, args);
}

inline auto open_file_async(multiplexer& multiplexer, filesystem_handle const& base, path_view const path, file_parameters const& args = {})
{
	return detail::open_file_async_impl(multiplexer, &base, path, args);
}

} // namespace allio
