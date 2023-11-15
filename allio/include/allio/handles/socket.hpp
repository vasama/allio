#pragma once

#include <allio/detail/handles/socket.hpp>

namespace allio {

using detail::socket_t;
using detail::abstract_socket_handle;

using detail::raw_socket_t;
using detail::abstract_raw_socket_handle;

namespace blocking { using namespace detail::_socket_b; }
namespace async { using namespace detail::_socket_a; }

} // namespace allio
