#pragma once

#include <allio/s2n/detail/s2n.hpp>

#include <vsm/result.hpp>

#include <s2n/ssl.h>

namespace allio::detail {

vsm::result<void> s2n_load_system_certificates(SSL_CTX* ssl_ctx);

} // namespace allio::detail
