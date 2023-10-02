#pragma once

#include <allio/detail/handles/stream_socket_handle.hpp>

namespace allio {

using stream_socket_handle = basic_blocking_handle<detail::_stream_socket_handle>;

template<typename Multiplexer>
using basic_stream_socket_handle = basic_async_handle<detail::_stream_socket_handle, Multiplexer>;


template<parameters<stream_socket_handle::connect_parameters> P = stream_socket_handle::connect_parameters::interface>
vsm::result<stream_socket_handle> connect(network_address const& address, P const& args = {})
{
	vsm::result<stream_socket_handle> r(vsm::result_value);
	vsm_try_void(r->connect(address, args));
	return r;
}

template<typename Multiplexer, parameters<stream_socket_handle::connect_parameters> P = stream_socket_handle::connect_parameters::interface>
auto connect_async(Multiplexer& multiplexer, network_address const& address, P const& args = {});

} // namespace allio
