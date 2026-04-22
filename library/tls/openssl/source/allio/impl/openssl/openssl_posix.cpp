#include <allio/impl/openssl/openssl.hpp>

using namespace allio;
using namespace allio::detail;

vsm::result<void> detail::openssl_load_system_certificates(SSL_CTX* const ssl_ctx)
{
	return {};
}
