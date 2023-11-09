#pragma once

#include <allio/detail/handles/socket_handle.hpp>

namespace allio {

using detail::socket_handle_t;
using detail::abstract_socket_handle;

using detail::raw_socket_handle_t;
using detail::abstract_raw_socket_handle;

namespace blocking { using namespace detail::_socket_blocking; }
namespace async { using namespace detail::_socket_async; }

} // namespace allio
