#pragma once

#include <allio/openssl/detail/openssl.hpp>

#include <vsm/result.hpp>

#include <openssl/ssl.h>

namespace allio::detail {

vsm::result<void> openssl_load_system_certificates(SSL_CTX* ssl_ctx);

} // namespace allio::detail
