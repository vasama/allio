#pragma once

#include <allio/handles/socket.hpp>
#include <allio/openssl/detail/socket.hpp>

namespace allio::openssl {

using socket_t = detail::openssl_socket_t;

} // namespace allio::openssl
