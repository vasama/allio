#pragma once

#include <allio/detail/default_security_provider.hpp>
#include <allio/detail/handle.hpp>
#include <allio/detail/no_security_provider.hpp>

#include <vsm/linear.hpp>
#include <vsm/standard.hpp>
#include <vsm/type_traits.hpp>

namespace allio::detail {

template<typename SecurityProvider = default_security_provider>
class socket_handle_base : handle
{
protected:
	using base_type = handle;

private:
	using security_provider_type = vsm::select_t<
		std::is_void_v<SecurityProvider>, no_security_provider, SecurityProvider>;

	using secure_socket_type = typename security_provider_type::socket_type;

	vsm::linear<native_platform_handle> m_platform_handle;
	vsm_no_unique_address secure_socket_type m_secure_socket;

public:
};

struct network_endpoint_t
{
	network_endpoint endpoint;
};

} // namespace allio::detail
