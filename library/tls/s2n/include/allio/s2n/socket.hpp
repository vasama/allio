#pragma once

#include <allio/network_security.hpp>
#include <allio/s2n/detail/socket.hpp>

namespace allio::s2n {

template<typename RawSocket>
using basic_socket_t = detail::a_s2n_socket_t<RawSocket>;

using socket_t = basic_socket_t<detail::raw_socket_t>;
using socket_security_context = detail::a_s2n_socket_security_context;

[[nodiscard]] vsm::result<socket_security_context> create_socket_security_context(auto&&... args)
{
	return socket_security_context::create(
		detail::make_args<detail::security_context_parameters>(vsm_forward(args)...));
}

} // namespace allio::s2n
