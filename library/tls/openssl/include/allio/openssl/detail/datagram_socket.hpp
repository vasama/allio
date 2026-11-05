#pragma once

#include <allio/detail/handles/raw_datagram_socket.hpp>

namespace allio::detail {

template<typename RawSocket>
struct openssl_datagram_socket_t : datagram_socket_base_t<object_t>
{
	using base_type = datagram_socket_base_t<object_t>;

	template<operation Operation>
	static constexpr auto& blocking_io = uniplexer_handle::blocking_io<openssl_datagram_socket_t, Operation>;
};

template<typename RawSocket>
struct native_handle<openssl_datagram_socket_t<RawSocket>> : native_handle<RawSocket>
{
	openssl_tunnel_state* openssl;
};

} // namespace allio::detail
