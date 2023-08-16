#pragma once

#include <allio/handles/platform_handle2.hpp>
#include <allio/network.hpp>

namespace allio {

class common_socket_handle : public platform_handle
{
protected:
	using base_type = platform_handle;

	allio_detail_default_lifetime(common_socket_handle);
};

} // namespace allio 
