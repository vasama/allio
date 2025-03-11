#pragma once

#include <allio/detail/handles/raw_common_socket_base.hpp>

#include <allio/impl/posix/socket.hpp>

namespace allio::posix {

[[nodiscard]] inline detail::handle_flags set_address_family(int const address_family)
{
	detail::handle_flags flags = detail::handle_flags::none;

	switch (address_family)
	{
	default:
		vsm_assert(address_family == AF_UNSPEC); //PRECONDITION

	case AF_UNIX:
		flags |= detail::raw_common_socket_base_t::flags::address_family_0;
		break;

	case AF_INET:
		flags |= detail::raw_common_socket_base_t::flags::address_family_1;
		break;

	case AF_INET6:
		flags |= detail::raw_common_socket_base_t::flags::address_family_0;
		flags |= detail::raw_common_socket_base_t::flags::address_family_1;
		break;
	}

	return flags;
}

[[nodiscard]] inline int get_address_family(detail::handle_flags const flags)
{
	if (flags[detail::raw_common_socket_base_t::flags::address_family_1])
	{
		if (flags[detail::raw_common_socket_base_t::flags::address_family_0])
		{
			return AF_INET6;
		}
		else
		{
			return AF_INET;
		}
	}
	else
	{
		if (flags[detail::raw_common_socket_base_t::flags::address_family_0])
		{
			return AF_UNIX;
		}
		else
		{
			return AF_UNSPEC;
		}
	}
}

[[nodiscard]] inline network_address_kind get_address_kind(detail::handle_flags const flags)
{
	return get_address_kind(get_address_family(flags));
}

} // namespace allio::posix
