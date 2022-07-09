#pragma once

#include <allio/impl/posix/socket.hpp>
#include <allio/linux/detail/socket.hpp>

#include <memory>
#include <new>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

inline posix::socket_address_union& new_address(detail::socket_address_storage& storage)
{
	static_assert(sizeof(detail::socket_address_storage) >= sizeof(posix::socket_address_union));
	static_assert(alignof(detail::socket_address_storage) >= alignof(posix::socket_address_union));
	return *new (storage.storage) posix::socket_address_union;
}

inline posix::socket_address_union& get_address(detail::socket_address_storage& storage)
{
	return *std::launder(reinterpret_cast<posix::socket_address_union*>(storage.storage));
}

} // namespace allio::linux

#include <allio/linux/detail/undef.i>
