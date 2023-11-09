#pragma once

#include <allio/detail/handles/datagram_handle.hpp>

namespace allio {

using detail::datagram_read_result;

using detail::datagram_socket_t;
using detail::abstract_datagram_socket_handle;

namespace blocking { using namespace _datagram_socket_blocking; }
namespace async { using namespace _datagram_socket_async; }

} // namespace allio
