#pragma once

#include <allio/detail/handles/datagram_socket.hpp>

namespace allio::detail {

struct a_s2n_connection;

struct a_s2n_datagram_socket_t : datagram_socket_base_t<object_t>
{
	using base_type = datagram_socket_base_t<object_t>;
};

template<typename RawSocket>
struct native_handle<a_s2n_datagram_socket_t<RawSocket>> : native_handle<RawSocket>
{
	a_s2n_connection* context;
};

} // namespace allio::detail
