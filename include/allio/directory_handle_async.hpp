#pragma once

#include <allio/directory_handle.hpp>

#include <allio/async.hpp>

#include <unifex/defer.hpp>
#include <unifex/just.hpp>
#include <unifex/let_value_with.hpp>
#include <unifex/sequence.hpp>

namespace allio {
namespace detail {

template<parameters<directory_stream_handle_base::fetch_parameters> P>
basic_sender<io::directory_stream_fetch> directory_stream_handle_base::fetch_async(P const& args)
{
	return { static_cast<directory_stream_handle&>(*this), args };
}


inline auto open_directory_stream_async_impl(multiplexer& multiplexer,
	filesystem_handle const* const base, input_path_view const path,
	directory_stream_handle::open_parameters const& args)
{
	struct context
	{
		directory_stream_handle handle;
		allio::multiplexer* multiplexer;
		filesystem_handle const* base;
		input_path_view path;
		directory_stream_handle::open_parameters args;
	};
	
	return error_into_except(unifex::let_value_with(
		[ctx = context{ directory_stream_handle(), &multiplexer, base, path, args }]() mutable -> context&
		{
			ctx.args.multiplexable = true;
			return ctx;
		},
		[](context& ctx)
		{
			return unifex::sequence(
				unifex::defer([&] { return result_into_error(unifex::just(ctx.handle.set_multiplexer(ctx.multiplexer))); }),
				unifex::defer([&] { return basic_sender<io::directory_stream_open_path>(ctx.handle, ctx.args, ctx.base, ctx.path); }),
				unifex::defer([&] { return unifex::just(static_cast<decltype(context::handle)&&>(ctx.handle)); })
			);
		}
	));
}

inline auto open_directory_stream_async_impl(multiplexer& multiplexer,
	directory_handle const* const directory,
	directory_stream_handle_base::open_parameters const& args)
{
	struct context
	{
		directory_stream_handle handle;
		allio::multiplexer* multiplexer;
		directory_handle const* directory;
		directory_stream_handle::open_parameters args;
	};
	
	return error_into_except(unifex::let_value_with(
		[ctx = context{ directory_stream_handle(), &multiplexer, directory, args }]() mutable -> context&
		{
			ctx.args.multiplexable = true;
			return ctx;
		},
		[](context& ctx)
		{
			return unifex::sequence(
				unifex::defer([&] { return result_into_error(unifex::just(ctx.handle.set_multiplexer(ctx.multiplexer))); }),
				unifex::defer([&] { return basic_sender<io::directory_stream_open_handle>(ctx.handle, ctx.args, ctx.directory); }),
				unifex::defer([&] { return unifex::just(static_cast<decltype(context::handle)&&>(ctx.handle)); })
			);
		}
	));
}

} // namespace detail

template<parameters<directory_stream_handle::open_parameters> P = directory_stream_handle::open_parameters::interface>
auto open_directory_stream_async(multiplexer& multiplexer, input_path_view const path, P const& args = {})
{
	return detail::open_directory_stream_async_impl(multiplexer, nullptr, path, args);
}

template<parameters<directory_stream_handle::open_parameters> P = directory_stream_handle::open_parameters::interface>
auto open_directory_stream_async(multiplexer& multiplexer, filesystem_handle const& base, input_path_view const path, P const& args = {})
{
	return detail::open_directory_stream_async_impl(multiplexer, &base, path, args);
}

template<parameters<directory_stream_handle::open_parameters> P = directory_stream_handle::open_parameters::interface>
auto open_directory_stream_async(multiplexer& multiplexer, directory_handle const& directory, P const& args = {})
{
	return detail::open_directory_stream_async_impl(multiplexer, &directory, args);
}

} // namespace allio
