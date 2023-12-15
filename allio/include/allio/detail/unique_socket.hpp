#pragma once

#include <allio/detail/platform.hpp>

#include <vsm/standard.hpp>
#include <vsm/unique_resource.hpp>

namespace allio::detail {

void close_wrapped_socket(native_platform_handle socket);

struct wrapped_socket_deleter
{
	void vsm_static_operator_invoke(native_platform_handle const socket)
	{
		return close_wrapped_socket(socket);
	}
};

using unique_wrapped_socket = vsm::unique_resource<
	native_platform_handle,
	wrapped_socket_deleter,
	native_platform_handle::null>;

} // namespace allio::detail
