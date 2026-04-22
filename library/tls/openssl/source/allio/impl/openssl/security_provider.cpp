#include <allio/detail/security_provider.hpp>

#include <allio/openssl/listen_socket.hpp>
#include <allio/openssl/socket.hpp>

#include <openssl/opensslv.h>

using namespace allio;
using namespace allio::detail;

namespace {

struct openssl_socket_provider : secure_socket_provider
{
	std::string_view name() const override
	{
		return "OpenSSL";
	}

	std::string_view version() const override
	{
		return OpenSSL_version(OPENSSL_FULL_VERSION_STRING);
	}

	socket_handle make_stream_socket() const override
	{
		return detail::make_any<socket_t, openssl::socket_t>();
	}

	listen_socket_handle make_listen_socket() const override
	{
		return detail::make_any<listen_socket_t, openssl::listen_socket_t>();
	}
};

static openssl_socket_provider openssl_provider;

[[maybe_unused]] static const char initializer =
	(secure_socket_provider::register_provider(openssl_provider), 0);

} // namespace
