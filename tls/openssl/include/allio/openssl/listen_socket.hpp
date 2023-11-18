#pragma once

#include <allio/handles/listen_socket.hpp>
#include <allio/openssl/detail/listen_socket.hpp>

namespace allio::openssl {

using listen_socket_t = detail::openssl_listen_socket_t;

} // namespace allio::openssl
