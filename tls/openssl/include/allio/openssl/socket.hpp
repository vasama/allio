#pragma once

#include <allio/handles/socket.hpp>
#include <allio/openssl/detail/socket.hpp>

namespace allio::openssl {

using socket_t = detail::openssl_socket_t;
using socket_security_context = detail::openssl_socket_security_context;

[[nodiscard]] vsm::result<socket_security_context> create_socket_security_context(auto&&... args)
{
	return socket_security_context::create(make_args<detail::security_context_parameters>(vsm_forward(args)...));
}

} // namespace allio::openssl
