#pragma once

#include <allio/socket_handle.hpp>

#include <allio/async.hpp>

#include <unifex/defer.hpp>
#include <unifex/just.hpp>
#include <unifex/let_value_with.hpp>
#include <unifex/sequence.hpp>

namespace allio {

template<parameters<common_socket_handle::create_parameters> Parameters>
basic_sender<io::socket_create> common_socket_handle::create_async(network_address_kind const address_kind, Parameters const& args)
{
	basic_sender<io::socket_create> sender = { *this, args, address_kind };
	sender.arguments().multiplexable = true;
	return sender;
}

namespace detail {

template<parameters<stream_socket_handle_base::create_parameters> Parameters>
basic_sender<io::socket_connect> stream_socket_handle_base::connect_async(network_address const& address, Parameters const& args)
{
	return { static_cast<stream_socket_handle&>(*this), args, address };
}

template<parameters<stream_socket_handle_base::byte_io_parameters> Parameters>
basic_sender<io::stream_scatter_read> stream_socket_handle_base::read_async(read_buffer const buffer, Parameters const& args)
{
	return { *this, args, buffer };
}

template<parameters<stream_socket_handle_base::byte_io_parameters> Parameters>
basic_sender<io::stream_scatter_read> stream_socket_handle_base::read_async(read_buffers const buffers, Parameters const& args)
{
	return { *this, args, buffers };
}

template<parameters<stream_socket_handle_base::byte_io_parameters> Parameters>
basic_sender<io::stream_gather_write> stream_socket_handle_base::write_async(write_buffer const buffer, Parameters const& args)
{
	return { *this, args, buffer };
}

template<parameters<stream_socket_handle_base::byte_io_parameters> Parameters>
basic_sender<io::stream_gather_write> stream_socket_handle_base::write_async(write_buffers const buffers, Parameters const& args)
{
	return { *this, args, buffers };
}

template<parameters<listen_socket_handle_base::listen_parameters> Parameters>
basic_sender<io::socket_listen> listen_socket_handle_base::listen_async(network_address const& address, Parameters const& args)
{
	return { static_cast<listen_socket_handle&>(*this), args, address };
}

template<parameters<listen_socket_handle_base::accept_parameters> Parameters>
basic_sender<io::socket_accept> listen_socket_handle_base::accept_async(Parameters const& args) const
{
	basic_sender<io::socket_accept> sender = { static_cast<listen_socket_handle const&>(*this), args };
	sender.arguments().multiplexable = true;
	return sender;
}

} // namespace detail

template<parameters<stream_socket_handle::connect_parameters> Parameters = stream_socket_handle::connect_parameters::interface>
auto connect_async(multiplexer& multiplexer, network_address const& address, Parameters const& args = {})
{
	struct context
	{
		stream_socket_handle handle;
		allio::multiplexer* multiplexer;
		network_address address;
		stream_socket_handle::connect_parameters args;
	};

	return error_into_except(unifex::let_value_with(
		[ctx = context{ stream_socket_handle(), &multiplexer, address, args }]() mutable -> context&
		{
			return ctx;
		},
		[](context& ctx)
		{
			return unifex::sequence(
				unifex::defer([&]
				{
					ctx.args.deadline = ctx.args.deadline.start();
					return result_into_error(unifex::just(ctx.handle.set_multiplexer(ctx.multiplexer)));
				}),
				unifex::defer([&] { return ctx.handle.create_async(ctx.address.kind(), ctx.args); }),
				unifex::defer([&] { return ctx.handle.connect_async(ctx.address, ctx.args); }),
				unifex::defer([&] { return unifex::just(static_cast<stream_socket_handle&&>(ctx.handle)); })
			);
		}
	));
}

template<parameters<listen_socket_handle::listen_parameters> Parameters = listen_socket_handle::listen_parameters::interface>
auto listen_async(multiplexer& multiplexer, network_address const& address, Parameters const& args = {})
{
	struct context
	{
		listen_socket_handle handle;
		allio::multiplexer* multiplexer;
		network_address address;
		listen_socket_handle::listen_parameters args;
	};

	return error_into_except(unifex::let_value_with(
		[ctx = context{ listen_socket_handle(), &multiplexer, address, args }]() mutable -> context&
		{
			return ctx;
		},
		[](context& ctx)
		{
			return unifex::sequence(
				unifex::defer([&]
				{
					ctx.args.deadline = ctx.args.deadline.start();
					return result_into_error(unifex::just(ctx.handle.set_multiplexer(ctx.multiplexer)));
				}),
				unifex::defer([&] { return ctx.handle.create_async(ctx.address.kind(), ctx.args); }),
				unifex::defer([&] { return ctx.handle.listen_async(ctx.address, ctx.args); }),
				unifex::defer([&] { return unifex::just(static_cast<listen_socket_handle&&>(ctx.handle)); })
			);
		}
	));
}

} // namespace allio
