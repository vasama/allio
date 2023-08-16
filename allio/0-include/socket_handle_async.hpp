#pragma once

#include <allio/socket_handle.hpp>

#include <allio/async.hpp>

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
	return initialize_handle<stream_socket_handle>(multiplexer, [=](auto& h) mutable
	{
		return detail::execution::let_value(
			h.create_async(address.kind(), args),
			[&] { return h.connect_async(address, args); }
		);
	});
}

template<parameters<listen_socket_handle::listen_parameters> Parameters = listen_socket_handle::listen_parameters::interface>
auto listen_async(multiplexer& multiplexer, network_address const& address, Parameters const& args = {})
{
	return initialize_handle<listen_socket_handle>(multiplexer, [=](auto& h) mutable
	{
		return detail::execution::let_value(
			h.create_async(address.kind(), args),
			[&] { return h.listen_async(address, args); }
		);
	});
}

} // namespace allio
