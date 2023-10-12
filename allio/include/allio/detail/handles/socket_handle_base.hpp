#pragma once

#include <allio/detail/default_security_provider.hpp>
#include <allio/detail/handles/platform_handle.hpp>
#include <allio/network.hpp>
#include <allio/platform.hpp>

#include <vsm/linear.hpp>
#include <vsm/standard.hpp>
#include <vsm/type_traits.hpp>

namespace allio::detail {

struct network_endpoint_t
{
	network_endpoint endpoint;
};

struct no_security_provider
{
	class socket_type
	{
	public:
		struct native_handle_type {};
	};
};

void close_socket_handle(native_platform_handle handle);

template<typename SecurityProvider = default_security_provider>
class socket_handle_base : public platform_handle
{
protected:
	using base_type = platform_handle;

	using security_provider_type = vsm::select_t<
		std::is_void_v<SecurityProvider>, no_security_provider, SecurityProvider>;

	using secure_socket_type = typename security_provider_type::socket_type;

	vsm_no_unique_address secure_socket_type m_secure_socket;

public:
	struct native_handle_type : base_type::native_handle_type
	{
		secure_socket_type socket;
	};

protected:
	static bool check_native_handle(native_handle_type const& native)
	{
		return base_type::check_native_handle(native);
	}

	void set_native_handle(native_handle_type&& native)
	{
		base_type::set_native_handle(native);
		m_secure_socket = vsm_move(native.socket);
	}

	void close()
	{
		//m_secure_socket.close();

		base_type::close([](native_platform_handle const handle)
		{
			close_socket_handle(handle);
		});
	}
};

} // namespace allio::detail
