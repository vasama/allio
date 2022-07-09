#pragma once

#include <allio/socket_handle.hpp>

#include <allio/async.hpp>

#include <unifex/defer.hpp>
#include <unifex/just.hpp>
#include <unifex/let_value_with.hpp>
#include <unifex/sequence.hpp>

namespace allio {

inline basic_sender<io::socket> detail::common_socket_handle_base::create_async(network_address_kind const address_kind, socket_parameters const& args)
{
	socket_parameters args_copy = args;
	args_copy.handle_flags |= flags::multiplexable;
	return { *this, address_kind, args_copy };
}

inline basic_sender<io::connect> detail::socket_handle_base::connect_async(network_address const& address)
{
	return { static_cast<socket_handle&>(*this), address };
}

inline basic_sender<io::stream_scatter_read> detail::socket_handle_base::read_async(read_buffer const buffer)
{
	return { *this, buffer };
}

inline basic_sender<io::stream_scatter_read> detail::socket_handle_base::read_async(read_buffers const buffers)
{
	return { *this, buffers };
}

inline basic_sender<io::stream_gather_write> detail::socket_handle_base::write_async(write_buffer const buffer)
{
	return { *this, buffer };
}

inline basic_sender<io::stream_gather_write> detail::socket_handle_base::write_async(write_buffers const buffers)
{
	return { *this, buffers };
}

inline basic_sender<io::listen> detail::listen_socket_handle_base::listen_async(network_address const& address, listen_parameters const& args)
{
	return { static_cast<listen_socket_handle&>(*this), address, args };
}

inline basic_sender<io::accept> detail::listen_socket_handle_base::accept_async(socket_parameters const& create_args) const
{
	socket_parameters create_args_copy = create_args;
	create_args_copy.handle_flags |= flags::multiplexable;
	return { static_cast<listen_socket_handle const&>(*this), create_args_copy };
}


inline auto connect_async(multiplexer& multiplexer, network_address const& address, socket_parameters const& create_args = {})
{
	struct context
	{
		socket_handle handle;
		allio::multiplexer* multiplexer;
		network_address address;
		socket_parameters create_args;
	};

	return error_into_except(unifex::let_value_with(
		[ctx = context{ socket_handle(), &multiplexer, address, create_args }]() mutable -> context&
		{
			return ctx;
		},
		[](context& ctx)
		{
			return unifex::sequence(
				unifex::defer([&] { return result_into_error(unifex::just(ctx.handle.set_multiplexer(ctx.multiplexer))); }),
				unifex::defer([&] { return ctx.handle.create_async(ctx.address.kind(), ctx.create_args); }),
				unifex::defer([&] { return ctx.handle.connect_async(ctx.address); }),
				unifex::defer([&] { return unifex::just(static_cast<decltype(context::handle)&&>(ctx.handle)); })
			);
		}
	));
}

inline auto listen_async(multiplexer& multiplexer, network_address const& address, listen_parameters const& args = {}, socket_parameters const& create_args = {})
{
	struct context
	{
		listen_socket_handle handle;
		allio::multiplexer* multiplexer;
		network_address address;
		listen_parameters args;
		socket_parameters create_args;
	};

	return error_into_except(unifex::let_value_with(
		[ctx = context{ listen_socket_handle(), &multiplexer, address, args, create_args }]() mutable -> context&
		{
			return ctx;
		},
		[](context& ctx)
		{
			return unifex::sequence(
				unifex::defer([&] { return result_into_error(unifex::just(ctx.handle.set_multiplexer(ctx.multiplexer))); }),
				unifex::defer([&] { return ctx.handle.create_async(ctx.address.kind(), ctx.create_args); }),
				unifex::defer([&] { return ctx.handle.listen_async(ctx.address, ctx.args); }),
				unifex::defer([&] { return unifex::just(static_cast<decltype(context::handle)&&>(ctx.handle)); })
			);
		}
	));
}

} // namespace allio
