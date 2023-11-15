#pragma once

#include <allio/detail/default_security_provider.hpp>
#include <allio/detail/handles/platform_object.hpp>
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

} // namespace allio::detail
