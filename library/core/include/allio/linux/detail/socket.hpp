#pragma once

namespace allio::detail {

struct socket_address_storage
{
	alignas(4) unsigned char storage[112];
};

} // namespace allio::detail
