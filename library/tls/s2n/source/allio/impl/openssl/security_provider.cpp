#include <allio/detail/security_provider.hpp>

#include <allio/s2n/listen_socket.hpp>
#include <allio/s2n/socket.hpp>

#include <s2n/s2nv.h>

using namespace allio;
using namespace allio::detail;

namespace {

struct s2n_socket_provider : secure_socket_provider
{
	std::string_view name() const override
	{
		return "s2n";
	}

	std::string_view version() const override
	{
		return OpenSSL_version(OPENSSL_FULL_VERSION_STRING);
	}

	socket_handle make_stream_socket() const override
	{
		return detail::make_any<socket_t, s2n::socket_t>();
	}

	listen_socket_handle make_listen_socket() const override
	{
		return detail::make_any<listen_socket_t, s2n::listen_socket_t>();
	}
};

static s2n_socket_provider s2n_provider;

[[maybe_unused]] static const char initializer =
	(secure_socket_provider::register_provider(s2n_provider), 0);

} // namespace
