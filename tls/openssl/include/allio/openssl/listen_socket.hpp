#pragma once

#include <allio/handles/listen_socket.hpp>
#include <allio/openssl/detail/listen_socket.hpp>

namespace allio::openssl {

using listen_socket_t = detail::openssl_listen_socket_t;
using listen_socket_security_context = detail::openssl_listen_socket_security_context;

[[nodiscard]] vsm::result<listen_socket_security_context> create_listen_socket_security_context(auto&&... args)
{
	return listen_socket_security_context::create(detail::make_args<detail::security_context_parameters>(vsm_forward(args)...));
}

} // namespace allio::openssl
