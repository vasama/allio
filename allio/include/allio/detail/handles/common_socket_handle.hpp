#pragma once

#include <allio/detail/handles/platform_handle.hpp>
#include <allio/network.hpp>

namespace allio::detail {

class common_socket_handle : public platform_handle
{
protected:
	using base_type = platform_handle;

	allio_detail_default_lifetime(common_socket_handle);

	void close();
};

} // namespace allio::detail
