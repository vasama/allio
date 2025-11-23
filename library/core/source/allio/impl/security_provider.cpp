#include <allio/detail/security_provider.hpp>

#include <allio/impl/service_provider_registry.hpp>

using namespace allio;
using namespace allio::detail;

namespace {

struct raw_socket_provider_impl : secure_socket_provider
{
};

static raw_socket_provider_impl const raw_socket_provider_instance;

} // namespace

static service_provider_registry<secure_socket_provider> g_registry;


void detail::register_socket_provider(secure_socket_provider const& provider)
{
}

[[nodiscard]] secure_socket_provider const& detail::raw_socket_provider()
{
	return raw_socket_provider_instance;
}

[[nodiscard]] secure_socket_provider const& detail::tls_socket_provider()
{
}
