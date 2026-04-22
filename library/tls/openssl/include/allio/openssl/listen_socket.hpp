#pragma once

#include <allio/handles/listen_socket_base.hpp>
#include <allio/network_security.hpp>
#include <allio/openssl/detail/listen_socket.hpp>

namespace allio::openssl {

template<typename RawListenSocket>
using basic_listen_socket_t = detail::openssl_listen_socket_t<RawListenSocket>;

using listen_socket_t = basic_listen_socket_t<detail::raw_listen_socket_t>;
using listen_socket_security_context = detail::openssl_listen_socket_security_context;

[[nodiscard]] vsm::result<listen_socket_security_context> create_listen_socket_security_context(
	auto&&... args)
{
	return listen_socket_security_context::create(
		detail::make_args<detail::security_context_parameters>(vsm_forward(args)...));
}

} // namespace allio::openssl
