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

#if 0
static constinit void* secure_socket_provider_head = nullptr;

void secure_socket_provider::register_provider_impl(secure_socket_provider& provider)
{
	provider.m_next_ptr = nullptr;

	auto p_next = vsm::atomic_ref<void*>(secure_socket_provider_head);
	while (true)
	{
		if (void* const next = p_next.load(std::memory_order_acquire))
		{
			p_next = vsm::atomic_ref<void*>(static_cast<secure_socket_provider>(next)->m_next_ptr);
		}
		else
		{
			void* expected = nullptr;
			if (p_next.compare_exchange_weak(
				expected,
				&provider,
				std::memory_order_release,
				std::memory_order_acquire))
			{
				break;
			}
		}
	}
}

void secure_socket_provider::load_default_providers()
{
	static constexpr auto initialize = []()
	{
		
	};

	[[maybe_unused]] static char const init = (initialize(), 0);
}

void secure_socket_provider::register_provider(secure_socket_provider& provider)
{
	load_default_providers();
	register_provider_impl(provider);
}

secure_socket_provider const* secure_socket_provider::head()
{
	load_default_providers();
	return secure_socket_provider_head.load(std::memory_order_acquire);
}

secure_socket_provider const* secure_socket_provider::next() const
{
	return static_cast<secure_socket_provider*>(m_next_ptr);
}

secure_socket_provider const* secure_socket_provider::get_default_provider()
{
	return head();
}
#endif
