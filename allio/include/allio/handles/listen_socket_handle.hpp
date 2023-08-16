#pragma once

#include <allio/detail/handles/listen_socket_handle.hpp>

namespace allio {

using detail::accept_result;


using listen_socket_handle = basic_handle<detail::_listen_socket_handle>;

template<typename Multiplexer>
using basic_listen_socket_handle = basic_async_handle<detail::_listen_socket_handle, Multiplexer>;


template<parameters<listen_parameters> P = listen_parameters::interface>
vsm::result<listen_socket_handle> listen(network_address const& address, P const& args = {});

template<typename Multiplexer, parameters<listen_parameters> P = listen_parameters::interface>
auto listen_async(Multiplexer& multiplexer, network_address const& address, P const& args = {});

} // namespace allio
