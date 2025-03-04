#pragma once

#include <allio/detail/handles/platform_object.hpp>
#include <allio/network.hpp>

namespace allio::detail {

struct raw_common_socket_base_t : platform_object_t
{
	using base_type = platform_object_t;

	allio_handle_flags
	(
		address_family_0,
		address_family_1,
	);

	static network_address_kind get_address_kind(native_handle<raw_common_socket_base_t> const& h);
};

} // namespace allio::detail
