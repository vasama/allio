#pragma once

#include <allio/detail/handles/datagram_socket.hpp>

namespace allio {

using detail::receive_result;

using detail::datagram_socket_t;
using detail::abstract_datagram_socket_handle;

using detail::raw_datagram_socket_t;
using detail::abstract_raw_datagram_socket_handle;

namespace blocking { using namespace detail::_datagram_socket_b; }
namespace async { using namespace detail::_datagram_socket_a; }

} // namespace allio
